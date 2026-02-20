# Changesets

This project uses [Changesets](https://github.com/changesets/changesets) for versioning.

## ⚠️ Not yet following semver

nx.js is not yet following semantic versioning. **All changesets should use `patch`** until we reach a stable 1.0 release.

```md
---
"@nx.js/runtime": patch
---

Description of change
```

Do **not** use `minor` or `major` bumps.
