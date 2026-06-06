# nx.js slim bootstrap forwarder (NSP)

A patched [nx-hbloader] that runs as the `main` NSO of an installable title
(NSP). At launch it resolves the shared nx.js runtime NRO from `sdmc:/nx.js/`
(reading the `[runtime] version` requirement from the title's own RomFS, then
semver-matching the installed `nxjs-v<version>.nro` — the same logic the NRO
launcher uses, in `../source/resolve.c`), then loads that runtime NRO via the
homebrew loader ABI (`svcMapProcessCodeMemory` + a `ConfigEntry[]`), passing
`argv` = `"<runtime>" "nsp:"`.

The runtime then sees `envIsNso() == false` (it was loaded as homebrew, not as
the title's NSO), and the `"nsp:"` argv marker tells it to mount **this title's**
RomFS — the app's `main.js` + assets — via `romfsMountFromCurrentProcess`, and
run `romfs:/main.js`.

This is what `@nx.js/nsp --slim` ships as the NSP's exefs `main`, so a slim NSP
is a few hundred KB instead of embedding the full ~21 MB runtime NSO.

## Build

```sh
make            # -> hbl.nso + hbl.npdm  (DEVKITPRO must be set)
```

CI builds these and stages them into `@nx.js/nsp`'s `dist/`.

## Credits

Based on:

- [switchbrew/nx-hbloader](https://github.com/switchbrew/nx-hbloader) (MIT)
- [Skywalker25/Forwarder-Mod](https://github.com/Skywalker25/Forwarder-Mod) /
  natinusala's nx-hbloader-retroarch-forwarder-mod — patches to forward to an
  arbitrary NRO.

[nx-hbloader]: https://github.com/switchbrew/nx-hbloader
