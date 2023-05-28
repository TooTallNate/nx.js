#!/bin/bash
set -euo pipefail

# Build JS runtime
pnpm bundle
mkdir -p romfs
cp -v ./packages/runtime/runtime.js ./romfs/runtime.js

# Build packages and example apps
pnpm build

# Build nx.js `.nro`
rm -f nxjs.nro
make
