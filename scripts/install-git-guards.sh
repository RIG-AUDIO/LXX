#!/usr/bin/env bash
# Install the repository's git guards.
#
#   * core.hooksPath = .githooks          (enables .githooks/pre-push)
#   * executable bits on hooks and scripts
#   * repository-local committer identity for RIG-AUDIO
#
# Everything is written with `git config --local`.  Nothing global is touched.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || {
  echo "not inside a git repository" >&2; exit 1; })"
cd "$ROOT"

PUBLIC_USER="RIG-AUDIO"
PUBLIC_REMOTE="https://github.com/RIG-AUDIO/LXX.git"
PUBLIC_EMAIL="344299581+RIG-AUDIO@users.noreply.github.com"

fail=0
err() { printf 'FAIL: %s\n' "$*" >&2; fail=1; }
ok()  { printf 'ok:   %s\n' "$*"; }

# 1. hooks path ------------------------------------------------------------
if [ ! -f .githooks/pre-push ]; then
  err ".githooks/pre-push is missing"
else
  chmod +x .githooks/pre-push
  git config --local core.hooksPath .githooks
  ok "core.hooksPath = $(git config --local core.hooksPath)"
fi

# 2. executable bits -------------------------------------------------------
for s in scripts/*.sh .githooks/*; do
  [ -f "$s" ] || continue
  chmod +x "$s"
  if [ -x "$s" ]; then ok "executable: $s"; else err "not executable: $s"; fi
done
chmod +x scripts/inspect-source-tree.py
[ -x scripts/inspect-source-tree.py ] && ok "executable: scripts/inspect-source-tree.py" \
  || err "not executable: scripts/inspect-source-tree.py"

# 3. repository-local identity --------------------------------------------
cur_name="$(git config --local user.name || true)"
cur_mail="$(git config --local user.email || true)"
if [ "$cur_name" != "$PUBLIC_USER" ]; then
  git config --local user.name "$PUBLIC_USER"
fi
if [ "$cur_mail" != "$PUBLIC_EMAIL" ]; then
  git config --local user.email "$PUBLIC_EMAIL"
fi
if [ "$(git config --local user.name)" = "$PUBLIC_USER" ] &&
   [ "$(git config --local user.email)" = "$PUBLIC_EMAIL" ]; then
  ok "local committer identity is $PUBLIC_USER <$PUBLIC_EMAIL>"
else
  err "local committer identity is not $PUBLIC_USER"
fi

# 4. remote ----------------------------------------------------------------
origin="$(git remote get-url origin 2>/dev/null || echo '')"
if [ "$origin" = "$PUBLIC_REMOTE" ]; then
  ok "origin is $PUBLIC_REMOTE"
else
  err "origin is '${origin:-<none>}', expected '$PUBLIC_REMOTE'"
fi

# 5. no credential-override configuration ----------------------------------
for k in credential.helper http.https://github.com/.extraheader; do
  if v="$(git config --local --get "$k" 2>/dev/null)"; then
    err "unexpected local git config $k = <set>"
  fi
done
ok "no local credential override configured"

# 6. identity script -------------------------------------------------------
if bash "$ROOT/scripts/verify-github-identity.sh"; then
  ok "GitHub identity"
else
  err "GitHub identity"
fi

if [ "$fail" -ne 0 ]; then
  printf '\nGit guard installation FAILED\n' >&2
  exit 1
fi
printf '\nGit guards installed.\n'
