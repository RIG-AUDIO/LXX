#!/usr/bin/env bash
# Publication gate.  Every check must pass before anything is pushed.
#
#   1. GitHub identity is the public identity
#   2. plugin formats are VST3-only
#   3. required publication files exist
#   4. no secrets, credentials, or private workspace paths
#   5. first-party identity in the build graph is LXX / RIG-AUDIO
#   6. no tracked build outputs; binaries are limited to the reviewed allowlist
#   7. license, notices and attribution are present and consistent
#   8. source manifest (scripts/inspect-source-tree.py)
#   9. external denylist, when LXX_PUBLICATION_DENYLIST is set
#
# The file set is "everything git can see": staged, committed, and untracked
# but not ignored.  That way the gate also works before the first commit.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT"

fail=0
err() { printf 'FAIL: %s\n' "$*" >&2; fail=1; }
ok()  { printf 'ok:   %s\n' "$*"; }
sec() { printf '\n== %s ==\n' "$*"; }

# --- the file set under review ---------------------------------------------
tree_files=()
while IFS= read -r f; do
  [ -n "$f" ] && tree_files+=("$f")
done < <(git ls-files --cached --others --exclude-standard)
if [ "${#tree_files[@]}" -eq 0 ]; then
  printf 'FAIL: the repository has no visible files\n' >&2
  exit 1
fi

# tree_has <fixed-string>  -> 0 if any file in the set contains it
tree_has() {
  local needle="$1" f
  for f in "${tree_files[@]}"; do
    [ -f "$f" ] || continue
    grep -Fq -- "$needle" "$f" 2>/dev/null && return 0
  done
  return 1
}

# --------------------------------------------------------------------------
sec "1. GitHub identity"
if bash "$ROOT/scripts/verify-github-identity.sh"; then
  ok "identity"
else
  err "identity"
fi

# --------------------------------------------------------------------------
sec "2. Plugin formats"
if bash "$ROOT/scripts/verify-plugin-formats.sh"; then
  ok "formats"
else
  err "formats"
fi

# --------------------------------------------------------------------------
sec "3. Required publication files"
required_files=(
  CMakeLists.txt
  README.md
  LICENSE
  THIRD_PARTY_NOTICES.md
  AGENTS.md
  ARCHITECTURE.md
  .gitignore
  .gitattributes
  .clang-tidy
  src/dsp/CMakeLists.txt
  src/plugin/CMakeLists.txt
  tests/CMakeLists.txt
  src/plugin/resources/Fonts/din1451alt.ttf
  src/plugin/resources/Fonts/OFL-1.1.txt
  src/dsp/sequencer/grids/PROVENANCE.txt
  tests/fuzz/dsp_fuzz.cpp
  tests/fuzz/CMakeLists.txt
  scripts/verify-github-identity.sh
  scripts/verify-plugin-formats.sh
  scripts/verify-publication.sh
  scripts/inspect-source-tree.py
  scripts/run_clang_tidy.py
  scripts/install-git-guards.sh
  scripts/lxx-git.sh
  scripts/lxx-gh.sh
  .githooks/pre-push
  .github/workflows/public-audit.yml
  .github/workflows/linux.yml
  .github/workflows/plugin-validation.yml
  .github/workflows/sanitizers.yml
  .github/workflows/static-analysis.yml
  .github/workflows/fuzz.yml
  .github/workflows/nightly-qualification.yml
)
for f in "${required_files[@]}"; do
  if [ -e "$ROOT/$f" ]; then ok "present: $f"; else err "missing: $f"; fi
done

# --------------------------------------------------------------------------
sec "4. Secrets, credentials, private paths"
# Public-safe patterns only.  Private and legacy brand markers are supplied by
# the maintainer-side denylist (LXX_PUBLICATION_DENYLIST) so that this file
# does not itself publish what it forbids.
secret_patterns=(
  'ghp_[A-Za-z0-9]{20,}'
  'gho_[A-Za-z0-9]{20,}'
  'ghu_[A-Za-z0-9]{20,}'
  'ghs_[A-Za-z0-9]{20,}'
  'github_pat_[A-Za-z0-9_]{20,}'
  'AKIA[0-9A-Z]{16}'
  '-----BEGIN [A-Z ]*PRIVATE KEY-----'
  'xox[baprs]-[A-Za-z0-9-]{10,}'
  '(password|passwd|api_key|apikey)[[:space:]]*[:=][[:space:]]*["'"'"'][^"'"'"']{8,}["'"'"']'
  '[A-Za-z]:\\(Users|Windows|Program Files)'
  '/home/[A-Za-z_][\w.-]*/\.(ssh|aws|gnupg|gnome-keyring)'
  '(GH_TOKEN|GITHUB_TOKEN|NPM_TOKEN)[[:space:]]*=[[:space:]]*[^$*]'
)

secret_hits=0
for f in "${tree_files[@]}"; do
  [ -f "$f" ] || continue
  case "$f" in
    *.ttf|*.otf|*.png|*.jpg|*.jpeg|*.gif|*.ico|*.pdf|*.zip|*.wav) continue ;;
  esac
  for pat in "${secret_patterns[@]}"; do
    if grep -nE -- "$pat" "$f" >/dev/null 2>&1; then
      err "token matching /$pat/ in $f"
      grep -nE -- "$pat" "$f" | head -3 >&2 || true
      secret_hits=1
    fi
  done
done
for f in "${tree_files[@]}"; do
  case "$f" in
    .env|.env.*|credentials|credentials.*|secrets|secrets.*|.ssh|*.pem|*.key)
      err "credential-looking path is visible: $f" ;;
  esac
done
if [ "$secret_hits" -eq 0 ]; then ok "no secret or private-path pattern in the file set"; fi

# --------------------------------------------------------------------------
sec "5. First-party identity in the build graph"
identity_checks=(
  'COMPANY_NAME "RIG-AUDIO"'
  'BUNDLE_ID com.rigaudio.lxx'
  'PRODUCT_NAME "LXX"'
  'project(LXX'
  'JucePlugin_Manufacturer="RIG-AUDIO"'
  'PLUGIN_MANUFACTURER_CODE RgAu'
  'PLUGIN_CODE Lxx1'
)
for c in "${identity_checks[@]}"; do
  if tree_has "$c"; then ok "identity declaration: $c"; else err "identity declaration not found: $c"; fi
done

# --------------------------------------------------------------------------
sec "6. Visible outputs and binary allowlist"
binary_allow=(
  'src/plugin/resources/Fonts/din1451alt.ttf'
)
for f in "${tree_files[@]}"; do
  [ -f "$f" ] || continue
  [ -s "$f" ] || continue
  grep -Iq . "$f" 2>/dev/null && continue
  allowed=0
  for a in "${binary_allow[@]}"; do
    [ "$f" = "$a" ] && allowed=1 && break
  done
  if [ "$allowed" -eq 0 ]; then err "unexpected binary: $f"; fi
done
ok "binary allowlist checked (${#binary_allow[@]} entries)"

for f in "${tree_files[@]}"; do
  case "$f" in
    */build/*|build/*|*/_deps/*|_deps/*|*/cmake-build-*|*.user|*.swp|*~)
      err "build or editor artefact visible: $f" ;;
  esac
done
ok "no build or editor artefact visible"

# --------------------------------------------------------------------------
sec "7. Licence, notices, attribution"
if grep -qF 'Foundation, either version 3 of the License, or (at your option) any later' LICENSE; then
  ok "GPLv3-or-later grant present in LICENSE"
else
  err "GPLv3-or-later grant missing from LICENSE"
fi
if grep -q 'GNU GENERAL PUBLIC LICENSE' LICENSE && [ "$(wc -l < LICENSE)" -gt 600 ]; then
  ok "full GPL v3 text bundled in LICENSE"
else
  err "full GPL v3 text missing from LICENSE"
fi
for needle in 'Emilie Gillet' 'Mutable Instruments' 'pichenettes/eurorack' 'Peter Wiegel' 'Open Font License' 'GPLv3'; do
  if grep -qF -- "$needle" THIRD_PARTY_NOTICES.md; then
    ok "notice: $needle"
  else
    err "notice missing: $needle"
  fi
done
if grep -qF 'Copyright (c) 2017, Peter Wiegel' src/plugin/resources/Fonts/OFL-1.1.txt; then
  ok "bundled font licence text present"
else
  err "bundled font licence text missing"
fi
hash="$(sha256sum src/plugin/resources/Fonts/din1451alt.ttf | cut -d' ' -f1)"
if [ "$hash" = "3cdbdd35c7637ff4a15d22bdcb8b952f48afadd2e41edb5ab29d80eaad8d48fd" ]; then
  ok "font binary matches the reviewed SHA-256"
else
  err "font binary SHA-256 is $hash, expected the reviewed digest"
fi
if grep -qF 'Copyright 2011 Emilie Gillet' src/dsp/sequencer/grids/grids_pattern_generator.h; then
  ok "Grids upstream copyright notice preserved"
else
  err "Grids upstream copyright notice missing"
fi
if grep -qF 'Copyright 2012 Emilie Gillet' src/dsp/sequencer/grids/grids_resources.cpp; then
  ok "Grids resources copyright notice preserved"
else
  err "Grids resources copyright notice missing"
fi

# --------------------------------------------------------------------------
sec "8. Source manifest"
if python3 "$ROOT/scripts/inspect-source-tree.py" --root "$ROOT" --check; then
  ok "source tree inspection"
else
  err "source tree inspection"
fi

# --------------------------------------------------------------------------
sec "9. External denylist"
if [ -n "${LXX_PUBLICATION_DENYLIST:-}" ]; then
  if [ -f "$LXX_PUBLICATION_DENYLIST" ]; then
    if python3 "$ROOT/scripts/inspect-source-tree.py" --root "$ROOT" \
         --denylist "$LXX_PUBLICATION_DENYLIST" --check; then
      ok "external denylist clean ($LXX_PUBLICATION_DENYLIST)"
    else
      err "external denylist findings"
    fi
  else
    err "LXX_PUBLICATION_DENYLIST is set but the file does not exist"
  fi
else
  printf 'skip: LXX_PUBLICATION_DENYLIST not set (maintainer-side check)\n'
fi

# --------------------------------------------------------------------------
if [ "$fail" -ne 0 ]; then
  printf '\n\nPUBLICATION GATE FAILED\n' >&2
  exit 1
fi
printf '\n\nPUBLICATION GATE PASSED (%d files reviewed)\n' "${#tree_files[@]}"
