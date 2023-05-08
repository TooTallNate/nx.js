#!/bin/bash
set -euo pipefail
#curl --upload-file nxjs.nro ftp://192.168.86.115:5000/switch/nxjs.nro --netrc
curl --netrc \
    --upload-file nxjs.nro ftp://192.168.86.115:5000/switch/nxjs.nro \
    --upload-file nxjs.js ftp://192.168.86.115:5000/switch/nxjs.js
