#!/usr/bin/env bash
# Identity-checked `gh` wrapper.
#
# Verifies that the active GitHub account is RIG-AUDIO before every call and
# refuses operations that would delete or clobber public content.
#
# Usage:  scripts/lxx-gh.sh <gh args...>
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bash "$HERE/verify-github-identity.sh" >/dev/null

# --- hard refusals ---------------------------------------------------------
joined="$*"

case "$joined" in
  *"repo delete"*|*"repo destroy"*|*"auth logout"*|*"auth switch"*)
    printf 'lxx-gh: REFUSED — "%s" is not permitted through this wrapper.\n' "$joined" >&2
    printf '  Account switching is done deliberately, never as a side effect.\n' >&2
    exit 1 ;;
esac

case "$joined" in
  *"release create"*|*"release delete"*|*"tag delete"*)
    printf 'lxx-gh: REFUSED — releases and tag deletion are out of scope.\n' >&2
    exit 1 ;;
esac

case "$joined" in
  *"workflow run"*)
    printf 'lxx-gh: note — workflow dispatch will run under RIG-AUDIO.\n' >&2 ;;
esac

exec gh "$@"
