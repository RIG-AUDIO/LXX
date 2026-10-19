#!/usr/bin/env bash
# Enforce the plugin-format contract:
#
#   * LXX is a VST3 instrument plugin and nothing else
#   * FORMATS must contain exactly "VST3"
#   * no Standalone / AU / AAX / VST2 target anywhere in the build graph
#   * no application-style JUCE target
#   * the plugin identity values are present and consistent
#   * no compiled plugin artifact is tracked in git
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
PLUGIN_CMAKE="$ROOT/src/plugin/CMakeLists.txt"
fail=0
err() { printf 'FAIL: %s\n' "$*" >&2; fail=1; }
ok()  { printf 'ok:   %s\n' "$*"; }

if [ ! -f "$PLUGIN_CMAKE" ]; then
  err "missing $PLUGIN_CMAKE"
  exit 1
fi

# ---- FORMATS must be exactly VST3 ----------------------------------------
formats_line="$(grep -E '^\s*FORMATS\b' "$PLUGIN_CMAKE" || true)"
if [ -z "$formats_line" ]; then
  err "no FORMATS entry in src/plugin/CMakeLists.txt"
else
  formats="$(printf '%s' "$formats_line" | sed -E 's/^[[:space:]]*FORMATS[[:space:]]+//')"
  if [ "$formats" = "VST3" ]; then
    ok "FORMATS is exactly: VST3"
  else
    err "FORMATS is '$formats'; expected exactly 'VST3'"
  fi
fi

# ---- No forbidden format token anywhere in the CMake graph ----------------
# JUCE_VST3_CAN_REPLACE_VST2=0 is a mandatory compile definition (it disables
# VST2 hosting/replacement); it is not a VST2 target, so strip it first.
FORBIDDEN_RE='\b(Standalone|AAX|AudioUnit|\bAU\b|VST2|AUv3)\b'
graph="$(find "$ROOT" -name 'CMakeLists.txt' -not -path '*/.git/*' -not -path '*/build/*' -print)"
while IFS= read -r f; do
  cleaned="$(sed 's/JUCE_VST3_CAN_REPLACE_VST2//g' "$f")"
  if printf '%s\n' "$cleaned" | grep -nE "$FORBIDDEN_RE" >/dev/null 2>&1; then
    err "forbidden plugin format token in $f:"
    printf '%s\n' "$cleaned" | grep -nE "$FORBIDDEN_RE" >&2 || true
  fi
done <<< "$graph"
if [ "$fail" -eq 0 ]; then
  ok "no Standalone / AU / AAX / VST2 token in any CMakeLists.txt"
fi

# ---- No application-style JUCE targets ------------------------------------
# juce_add_gui_app / juce_add_plugin(...) with Standalone would create a
# runnable application. juce_add_console_app is permitted ONLY for the
# documented diagnostic harness, which is never packaged or published.
allow_console="LXXHostRuntimeTest"
console_count=0
while IFS= read -r f; do
  while IFS= read -r target; do
    [ -z "$target" ] && continue
    console_count=$((console_count + 1))
    if [ "$target" != "$allow_console" ]; then
      err "application-style target '$target' declared in $f (only $allow_console is allowed)"
    else
      ok "console diagnostic harness '$target' in $f (not packaged, not published)"
    fi
  done < <(sed -nE 's/.*juce_add_(gui_app|console_app)[[:space:]]*\(([A-Za-z0-9_]+).*/\2/p' "$f")
done <<< "$graph"
if [ "$console_count" -ne 1 ]; then
  err "expected exactly 1 diagnostic console target, found $console_count"
fi

# ---- Plugin identity ------------------------------------------------------
check_decl() {
  local pat="$1" desc="$2"
  if grep -Eq "$pat" "$PLUGIN_CMAKE"; then ok "$desc"; else err "$desc not found"; fi
}
check_decl '^[[:space:]]*PLUGIN_MANUFACTURER_CODE[[:space:]]+RgAu'   "PLUGIN_MANUFACTURER_CODE RgAu"
check_decl '^[[:space:]]*PLUGIN_CODE[[:space:]]+Lxx1'                "PLUGIN_CODE Lxx1"
check_decl '^[[:space:]]*BUNDLE_ID[[:space:]]+com\.rigaudio\.lxx'    "BUNDLE_ID com.rigaudio.lxx"
check_decl '^[[:space:]]*COMPANY_NAME[[:space:]]+"RIG-AUDIO"'        'COMPANY_NAME "RIG-AUDIO"'
check_decl '^[[:space:]]*IS_SYNTH[[:space:]]+TRUE'                   "IS_SYNTH TRUE"
check_decl '^[[:space:]]*VST3_CATEGORIES[[:space:]]+Instrument Drum' "VST3_CATEGORIES Instrument Drum"
check_decl 'JucePlugin_ManufacturerCode=0x52674175'                  "diagnostic harness manufacturer code in sync"
check_decl 'JucePlugin_PluginCode=0x4c787831'                        "diagnostic harness plugin code in sync"

# ---- JUCE pinned ----------------------------------------------------------
root_cmake="$ROOT/CMakeLists.txt"
if grep -Eq 'GIT_TAG[[:space:]]+7\.0\.10' "$root_cmake"; then
  ok "JUCE pinned to 7.0.10"
else
  err "JUCE is not pinned to a tag in CMakeLists.txt"
fi

# ---- No compiled plugin artifacts tracked ---------------------------------
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  bad="$(git -C "$ROOT" ls-files -z | tr '\0' '\n' \
        | grep -E '\.(vst3|component|dll|so|dylib|exe)$' || true)"
  if [ -n "$bad" ]; then
    err "compiled plugin artifact(s) tracked in git:"$'\n'"$bad"
  else
    ok "no compiled plugin artifact tracked in git"
  fi
fi

if [ "$fail" -ne 0 ]; then
  printf '\nPlugin format verification FAILED.\n' >&2
  exit 1
fi
printf '\nPlugin format verification PASSED.\n'
