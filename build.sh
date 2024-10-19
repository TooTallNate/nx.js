#!/bin/bash
set -euo pipefail

APP="${1-hello-world}"

# Build JS runtime
pnpm bundle

# Build packages and example app
pnpm build --filter "$APP"

# Build nx.js `.nro`
rm -f nxjs.nro
make

# Package app `.nro`
pnpm nro --filter "$APP"

if [ -n "${UPLOAD-}" ]; then
	app_path="$(pnpm list -r --depth -1 --json | jq -r '.[] | select(.name=="'"${APP}"'") | .path')"
	nro_name="$(jq -r '.productName // .name' "${app_path}/package.json")"
	curl --netrc "ftp://192.168.1.249:5000/switch/" --upload-file "${app_path}/${nro_name}.nro"
fi
