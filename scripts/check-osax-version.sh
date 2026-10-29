#!/usr/bin/env bash
#
# Fail when the scripting-addition sources changed since the last release but OSAX_VERSION did not.
# `yabai --load-sa` only reinstalls the payload when OSAX_VERSION differs from the installed one,
# so a payload change without a version bump never reaches users who already have it installed.
#
# Usage: scripts/check-osax-version.sh [base-ref]   (default: the most recent tag)
#

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

BASE="${1:-$(git describe --tags --abbrev=0 HEAD)}"
VERSION_FILE="src/osax/common.h"

osax_version() {
    sed -n 's/^#define OSAX_VERSION[[:space:]]*"\(.*\)"/\1/p'
}

if git diff --quiet "$BASE" -- src/osax; then
    echo "osax: no changes since $BASE"
    exit 0
fi

BASE_VERSION=$(git show "$BASE:$VERSION_FILE" | osax_version)
CURRENT_VERSION=$(osax_version < "$VERSION_FILE")

if [ "$BASE_VERSION" = "$CURRENT_VERSION" ]; then
    echo "error: src/osax changed since $BASE, but OSAX_VERSION is still $CURRENT_VERSION." >&2
    echo "       Bump OSAX_VERSION in $VERSION_FILE, or users keep the old payload installed." >&2
    git diff --stat "$BASE" -- src/osax >&2
    exit 1
fi

echo "osax: changed since $BASE, OSAX_VERSION $BASE_VERSION -> $CURRENT_VERSION"
