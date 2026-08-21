#!/bin/bash
# Lists public API added since a tag that no user-facing documentation mentions.
#
# The release checklist asks whether wirestead-docs was updated when the public
# API changed. That box was tickable without checking, and it was ticked while
# v0.9.4 and v0.9.5 added eight APIs the documentation site never mentioned. So
# this answers the question mechanically instead of asking a human to remember.
#
#   scripts/check_docs_coverage.sh [since-tag] [path-to-wirestead-docs]
#
# With no docs path it does a shallow clone into a temporary directory. Both
# this repository's docs/ and the site are searched: an API documented in either
# is reported as covered, since the two split the work between them.
#
# The extraction is deliberately crude - it reports identifiers, not semantics -
# so expect entries that need no prose. Read the list, do not obey it.
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

SINCE="${1:-$(git describe --tags --abbrev=0 2>/dev/null || echo "")}"
DOCS_PATH="${2:-}"

if [ -z "$SINCE" ]; then
  echo "No tag to compare against. Pass one explicitly." >&2
  exit 2
fi

if ! git rev-parse --verify "$SINCE" >/dev/null 2>&1; then
  echo "Not a valid ref: $SINCE" >&2
  exit 2
fi

CLEANUP_DIR=""
if [ -z "$DOCS_PATH" ]; then
  CLEANUP_DIR="$(mktemp -d)"
  trap 'rm -rf "$CLEANUP_DIR"' EXIT
  echo "Cloning wirestead-docs..." >&2
  git clone --depth 1 --quiet https://github.com/wirestead/wirestead-docs.git \
    "$CLEANUP_DIR/docs" 2>/dev/null || {
      echo "Could not clone wirestead-docs; pass a local checkout as the second argument." >&2
      exit 2
    }
  DOCS_PATH="$CLEANUP_DIR/docs"
fi

if [ ! -d "$DOCS_PATH" ]; then
  echo "Not a directory: $DOCS_PATH" >&2
  exit 2
fi

# Identifiers introduced on added lines of the headers a user actually calls -
# builders, wrappers, framers, the concurrency hook, shared base types. Transport
# and interface headers are excluded on purpose: they are full of internals whose
# names would drown the signal, and nothing there is API a reader looks up.
#
# Names ending in an underscore are dropped as member variables by convention.
# The file list is built first and filtered to headers, rather than passed to
# git diff as a glob: 'dir/**/*.hpp' does not match 'dir/x.hpp' in a git
# pathspec, which silently skipped every builder header on the first attempt.
mapfile -t HEADERS < <(
  git diff --name-only "$SINCE"..HEAD -- \
      wirestead/builder wirestead/wrapper wirestead/framer \
      wirestead/concurrency wirestead/base |
    grep '\.hpp$'
)

if [ ${#HEADERS[@]} -eq 0 ]; then
  echo "No user-facing headers changed since $SINCE."
  exit 0
fi

mapfile -t NAMES < <(
  git diff "$SINCE"..HEAD -- "${HEADERS[@]}" |
    grep '^+' | grep -v '^+++' |
    grep -oE '\b[a-z_][a-z0-9_]{3,}\s*\(' |
    tr -d ' (' |
    grep -vE '_$' |
    grep -vE '^(if|for|while|switch|return|sizeof|catch|and|or|not|else|case|throw|delete|decltype|static_cast|reinterpret_cast|const_cast|dynamic_cast|make_shared|make_unique|move|forward|size|data|empty|begin|end|value|count|find|push_back|emplace_back|lock|unlock|load|store|get|set)$' |
    sort -u
)

if [ ${#NAMES[@]} -eq 0 ]; then
  echo "No new public API identifiers since $SINCE."
  exit 0
fi

missing=()
for name in "${NAMES[@]}"; do
  if grep -rqw "$name" docs/ README.md 2>/dev/null; then continue; fi
  if grep -rqw "$name" "$DOCS_PATH" --include='*.md' 2>/dev/null; then continue; fi
  missing+=("$name")
done

echo "Public API identifiers added since $SINCE: ${#NAMES[@]}"
echo "Documented in this repository's docs/ or in wirestead-docs: $(( ${#NAMES[@]} - ${#missing[@]} ))"

if [ ${#missing[@]} -eq 0 ]; then
  echo
  echo "Nothing undocumented."
  exit 0
fi

echo
echo "No mention anywhere:"
printf '  %s\n' "${missing[@]}"
echo
echo "Each of these is either an API that needs prose or a false positive from"
echo "the crude extraction. Decide per entry; do not bulk-document them."
exit 1
