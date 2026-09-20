#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "==> 1. Building optimized release binary (make install)..."
make install

echo "==> 2. Signing with Developer ID Application and Hardened Runtime..."
make sign

VERSION=$(./bin/yabai --version)
echo "==> Version: $VERSION"

echo "==> 3. Creating release archive (make archive)..."
make archive

ARCHIVE="bin/${VERSION}.tar.gz"
if [[ -f "$ARCHIVE" ]]; then
    SHA=$(shasum -a 256 "$ARCHIVE" | awk '{print $1}')
    echo "$SHA  $(basename "$ARCHIVE")" > "${ARCHIVE}.sha256"
    echo "==> Archive created: $ARCHIVE"
    echo "==> SHA-256: $SHA"
fi

echo "==> 4. Checking Apple Notary Service status..."
xcrun notarytool history --keychain-profile "yabai-profile" 2>/dev/null || true

echo "==> Done! Ready for release distribution."
