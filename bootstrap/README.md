# nx.js slim bootstrap launcher

A tiny homebrew NRO (pure C, libnx only — no V8/Skia) that does **not** embed the
nx.js runtime. It chainloads a **shared** runtime NRO installed under
`sdmc:/nx.js/` and hands it the app to run.

This is the base used by `@nx.js/nro --slim`: the slim builder takes
`bootstrap.nro`, stages the app's `main.js` + assets + `nxjs.ini` into its RomFS,
and ships a NRO that's a few hundred KB instead of ~40 MB.

## How it works

1. The slim app NRO is launched from hbmenu.
2. This launcher reads `[runtime] version` (a semver requirement) from its own
   `romfs:/nxjs.ini` (default baked at build time: `^<runtime-major>`).
3. It scans `sdmc:/nx.js/nxjs-v<full-version>.nro` and picks the **highest
   installed version** that satisfies the requirement.
4. `envSetNextLoad(<runtime>, "\"<runtime>\" \"<self.nro>\"")` + exit.
5. hbloader loads the runtime with `argv[1]` = this NRO; the runtime mounts this
   NRO's RomFS as `romfs:` and runs `romfs:/main.js`.

If no compatible runtime is found (or it wasn't launched via hbloader), it shows
an on-screen error and waits for **+** — it never crashes.

## Build

```sh
make            # produces bootstrap.nro (DEVKITPRO must be set)
```

`jq` derives the runtime major from `../packages/runtime/package.json` for the
baked default version requirement.

## Test

The version-matching logic (`source/match.c`) is split from libnx so it can run
on the host without a Switch:

```sh
test/run.sh
```

## Vendored libraries

- `source/vendor/semver.c` / `semver.h` — [h2non/semver.c](https://github.com/h2non/semver.c) (MIT)
- `source/vendor/ini.h` — [benhoyt/inih](https://github.com/benhoyt/inih) (BSD-3), single-header
