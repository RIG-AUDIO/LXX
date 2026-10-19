#!/usr/bin/env bash
# Identity-checked `git` wrapper.
#
# Refuses to run anything through git unless the active GitHub account is
# RIG-AUDIO and this repository's origin is the LXX public remote, and it
# hard-refuses force-push.
#
# Usage:  scripts/lxx-git.sh <git args...>
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bash "$HERE/verify-github-identity.sh" >/dev/null

# --- hard refusals ---------------------------------------------------------
force=0
subcmd=""
for arg in "$@"; do
  case "$arg" in
    --force|-f|--force-with-lease|--force-if-includes) force=1 ;;
  esac
done
for arg in "$@"; do
  case "$arg" in
    -*) ;;
    *) subcmd="$arg"; break ;;
  esac
done

if [ "$force" -eq 1 ]; then
  printf 'lxx-git: REFUSED — force-push is never allowed.\n' >&2
  exit 1
fi

if [ "$subcmd" = "push" ]; then
  dest=""
  prev=""
  for arg in "$@"; do
    if [ "$prev" = "push" ]; then dest="$arg"; fi
    prev="$arg"
  done
  if [ -n "$dest" ]; then
    case "$dest" in
      origin|https://github.com/RIG-AUDIO/LXX.git|https://github.com/RIG-AUDIO/LXX) ;;
      *)
        printf 'lxx-git: REFUSED — refusing to push to "%s".\n' "$dest" >&2
        printf '  Only origin (https://github.com/RIG-AUDIO/LXX.git) is writable.\n' >&2
        exit 1 ;;
    esac
  fi
fi

exec git "$@"
