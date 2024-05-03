#!/bin/bash
set -euo pipefail

APP="${1-hello-world}"

# Build JS runtime
pnpm bundle
mkdir -p romfs
cp -v ./packages/runtime/runtime.* ./romfs/

# Build packages and example app
pnpm build --filter "$APP"

# Build nx.js `.nro`
rm -f nxjs.nro
make

# Package app `.nro`
pnpm nro --filter "$APP"

if [ -n "${UPLOAD-}" ]; then
	app_path="$(pnpm list -r --depth -1 --json | jq -r '.[] | select(.name=="'"${APP}"'") | .path')"
	curl --netrc "ftp://192.168.86.115:5000/switch/${APP}.nro" --upload-file "${app_path}/${APP}.nro"
fi
