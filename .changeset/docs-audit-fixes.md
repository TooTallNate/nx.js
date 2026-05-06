---
"@nx.js/runtime": patch
"@nx.js/constants": patch
---

docs: address documentation audit findings

- Fix Save Data concept example to use the synchronous `Switch.readFileSync` /
  `Switch.writeFileSync` APIs (the previous example referenced a non-existent
  `Switch.writeFile`).
- Add a `/runtime/concepts` index page so the Concepts section header in the
  sidebar no longer 404s.
- Backfill TSDoc descriptions on `SaveData` properties, `RequestInit` fields
  (with explicit honored / ignored notes for the Switch runtime), and the
  `Versions` interface.
- Convert the libnx-style `///<` and `/* … */` comments throughout
  `@nx.js/constants` (`FsSaveDataType`, `FsSaveDataSpaceId`, `Button`,
  `HidNpadButton`, `AppletType`, `OperationMode`, errno constants, etc.) to
  TSDoc so the auto-generated reference renders meaningful descriptions
  instead of `-`.
- Fix `kluer` → `kleur` typo on the Console rendering page.
