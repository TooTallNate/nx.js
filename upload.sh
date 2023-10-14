#!/usr/bin/env bash
set -euo pipefail
APP="${1-hello-world}"
SWITCH_HOST="${2-192.168.86.115:5000}"

ARGS=(
    --upload-file nxjs.nro "ftp://${SWITCH_HOST}/switch/nxjs.nro"
    --upload-file "./apps/${APP}/romfs/main.js" "ftp://${SWITCH_HOST}/switch/nxjs.js"
)

# Upload source map file, if it exists
if [ -f "./apps/${APP}/romfs/main.js.map" ]; then
    ARGS+=(--upload-file "./apps/${APP}/romfs/main.js.map" "ftp://${SWITCH_HOST}/switch/main.js.map")
fi

curl --netrc-optional "${ARGS[@]}"
