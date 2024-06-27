#!/usr/bin/env bash
set -euo pipefail
cd ..
rm -rf docs/content/docs/api
mkdir -p docs/content/docs/api
for d in packages/*/docs; do
    cp -rv "$d" "docs/content/docs/api/$(basename "$(dirname "$d")")"
done