#!/usr/bin/env bash
# Verify that the GitHub identity about to be used is the public identity.
#
# Two modes:
#
#   dev (default) — run on a maintainer machine.  PASS requires all of:
#     * gh is installed and authenticated
#     * the ACTIVE gh account is RIG-AUDIO
#     * `gh api user` agrees with that
#     * the git credential helper resolves github.com to RIG-AUDIO
#     * no GH_TOKEN / GITHUB_TOKEN environment override is present
#     * this repository's origin is exactly https://github.com/RIG-AUDIO/LXX.git
#
#   ci  (GITHUB_ACTIONS=true) — run by GitHub Actions.  There is no gh login
#     session and no git credential helper; identity is instead proved by the
#     workflow token addressing exactly the public repository:
#     * a GitHub token is present for this job (never printed)
#     * GITHUB_REPOSITORY is RIG-AUDIO/LXX
#     * the API reports RIG-AUDIO/LXX, public
#     * origin is RIG-AUDIO/LXX (with or without the .git suffix, which is how
#       actions/checkout records it)
set -euo pipefail

PUBLIC_USER="RIG-AUDIO"
PUBLIC_REPO="RIG-AUDIO/LXX"
PUBLIC_REMOTE="https://github.com/RIG-AUDIO/LXX.git"

# Never fall back to an interactive prompt inside hooks or CI.
export GIT_TERMINAL_PROMPT=0

fail=0
err() { printf 'FAIL: %s\n' "$*" >&2; fail=1; }
ok()  { printf 'ok:   %s\n' "$*"; }

# origin is RIG-AUDIO/LXX with an optional .git suffix
origin_matches() {
  case "$1" in
    "$PUBLIC_REMOTE"|"${PUBLIC_REMOTE%.git}") return 0 ;;
    *) return 1 ;;
  esac
}

if [ "${GITHUB_ACTIONS:-}" = "true" ]; then
  # ---------------- CI mode -------------------------------------------------
  if [ -z "${GH_TOKEN:-}${GITHUB_TOKEN:-}" ]; then
    err "CI run has no GitHub token (GH_TOKEN / GITHUB_TOKEN)"
  else
    ok "workflow token present (value not shown)"
  fi

  if [ "${GITHUB_REPOSITORY:-}" != "$PUBLIC_REPO" ]; then
    err "GITHUB_REPOSITORY is '${GITHUB_REPOSITORY:-<none>}', expected '$PUBLIC_REPO'"
  else
    ok "GITHUB_REPOSITORY is $PUBLIC_REPO"
  fi

  if ! command -v gh >/dev/null 2>&1; then
    err "gh is not installed"
  else
    repo_json="$(gh api "repos/$PUBLIC_REPO" 2>/dev/null || true)"
    api_name="$(printf '%s' "$repo_json" | sed -n 's/.*"full_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')"
    api_private="$(printf '%s' "$repo_json" | sed -n 's/.*"private"[[:space:]]*:[[:space:]]*\(true\|false\).*/\1/p')"
    if [ "$api_name" != "$PUBLIC_REPO" ]; then
      err "API repository is '${api_name:-<none>}', expected '$PUBLIC_REPO'"
    else
      ok "API repository is $api_name"
    fi
    if [ "$api_private" != "false" ]; then
      err "repository must be public (private=${api_private:-<unknown>})"
    else
      ok "repository is public"
    fi
  fi

  repo_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
  if [ -n "$repo_root" ]; then
    origin="$(git -C "$repo_root" remote get-url origin 2>/dev/null || true)"
    if ! origin_matches "$origin"; then
      err "origin is '${origin:-<none>}', expected '$PUBLIC_REMOTE'"
    else
      ok "origin is $origin"
    fi
  else
    err "not inside a git repository"
  fi

  if [ "$fail" -ne 0 ]; then
    printf '\nGitHub identity verification FAILED (CI mode).\n' >&2
    exit 1
  fi
  printf '\nGitHub identity verification PASSED (CI mode).\n'
  exit 0
fi

# ------------------------------ dev mode -----------------------------------
if [ -n "${GH_TOKEN:-}" ]; then
  err "GH_TOKEN is set in the environment; unset it and use 'gh auth switch'"
fi
if [ -n "${GITHUB_TOKEN:-}" ]; then
  err "GITHUB_TOKEN is set in the environment; unset it and use 'gh auth switch'"
fi

if ! command -v gh >/dev/null 2>&1; then
  err "gh is not installed"
  exit 1
fi

if ! status_out="$(gh auth status --hostname github.com 2>&1)"; then
  err "gh is not authenticated to github.com"
  printf '%s\n' "$status_out" >&2
  exit 1
fi

active="$(printf '%s\n' "$status_out" \
  | sed -n 's/^[[:space:]]*✓[[:space:]]*Logged in to github.com account \([^[:space:]]*\).*/\1/p' \
  | head -n1)"

if ! printf '%s\n' "$status_out" | grep -q 'Active account: true'; then
  err "no active gh account for github.com"
fi

if [ "$active" != "$PUBLIC_USER" ]; then
  err "active gh account is '${active:-<none>}', expected '$PUBLIC_USER'"
else
  ok "active gh account is $PUBLIC_USER"
fi

login="$(gh api user --jq .login 2>/dev/null || true)"
if [ "$login" != "$PUBLIC_USER" ]; then
  err "gh api user returned '${login:-<none>}', expected '$PUBLIC_USER'"
else
  ok "gh api user is $login"
fi

cred_out="$( (printf 'protocol=https\nhost=github.com\n' | git credential fill) 2>/dev/null || true)"
cred_user="$(printf '%s\n' "$cred_out" | sed -n 's/^username=//p')"
if [ "$cred_user" != "$PUBLIC_USER" ]; then
  err "git credential helper resolved '$cred_user', expected '$PUBLIC_USER'"
else
  ok "git credential helper resolves to $cred_user"
fi

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [ -n "$repo_root" ]; then
  origin="$(git -C "$repo_root" remote get-url origin 2>/dev/null || true)"
  if [ "$origin" != "$PUBLIC_REMOTE" ]; then
    err "origin is '${origin:-<none>}', expected '$PUBLIC_REMOTE'"
  else
    ok "origin is $PUBLIC_REMOTE"
  fi
  if [ -n "${LXX_ALLOW_ANY_ORIGIN:-}" ]; then
    ok "(origin check relaxed by LXX_ALLOW_ANY_ORIGIN)"
  fi
else
  err "not inside a git repository"
fi

if [ "$fail" -ne 0 ]; then
  printf '\nGitHub identity verification FAILED.\n' >&2
  printf "  gh auth switch --hostname github.com --user %s\n" "$PUBLIC_USER" >&2
  exit 1
fi

printf '\nGitHub identity verification PASSED.\n'
