#!/bin/bash
set -euo pipefail
SWITCH_HOST="${1-192.168.86.115:5000}"
curl --netrc-optional \
    --upload-file nxjs.nro "ftp://${SWITCH_HOST}/switch/nxjs.nro" \
    --upload-file nxjs.js "ftp://${SWITCH_HOST}/switch/nxjs.js"
