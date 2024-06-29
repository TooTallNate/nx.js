#!/usr/bin/env bash
API_DIR="docs/content/api"
set -euo pipefail
cd ..
rm -rf "$API_DIR"
cp -r docs/api.template "$API_DIR"
for d in packages/*/docs; do
    cp -rv "$d" "${API_DIR}/$(basename "$(dirname "$d")")"
done
