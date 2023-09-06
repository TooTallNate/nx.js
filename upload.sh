#!/bin/bash
set -euo pipefail
APP="${1-hello-world}"
SWITCH_HOST="${2-192.168.86.115:5000}"
curl --netrc-optional \
    --upload-file nxjs.nro "ftp://${SWITCH_HOST}/switch/nxjs.nro" \
    --upload-file "./apps/${APP}/romfs/main.js" "ftp://${SWITCH_HOST}/switch/nxjs.js"
