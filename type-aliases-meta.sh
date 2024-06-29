#!/usr/bin/env bash
set -euo pipefail
find docs -type d -name "type-aliases" | xargs -I {} cp -v ../../type-aliases.meta.json {}/meta.json
