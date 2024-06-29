#!/usr/bin/env bash
set -euo pipefail

CONTENT_DIR="docs/content"
cd ..
for d in packages/*/docs; do
    PKG="$(basename "$(dirname "$d")")"
    API_DIR="$CONTENT_DIR/$PKG/api"
    rm -rf "$API_DIR"
    mkdir -p "$API_DIR"
    cp -rv "$d"/* "$API_DIR"
done
