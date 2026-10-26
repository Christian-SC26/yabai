#!/usr/bin/env bash
IMAGE="$1"
TARGET="$2"
if [ -f "$IMAGE" ] && [ -f "$TARGET" ]; then
    swift -e '
    import AppKit
    if let img = NSImage(contentsOfFile: CommandLine.arguments[1]) {
        _ = NSWorkspace.shared.setIcon(img, forFile: CommandLine.arguments[2], options: [])
    }
    ' "$IMAGE" "$TARGET" 2>/dev/null || true
fi
