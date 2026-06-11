---
"@nx.js/runtime": patch
---

fix: `drawImage()` type confusion between wrapped Canvas and Image sources

All wrapped native objects share one ObjectTemplate, so `drawImage()`
duck-typed every source as an `nx_image_t` — a canvas source aliased its
`surface_dirty`/`gpu` flags with the image's `cached_sk_image` slot, turning
a bool into a dereferenced pointer (a crash in the console present path,
which draws the terminal canvas every frame in console-only apps). Both
structs now carry a leading magic discriminator validated by
`nx_get_image()`/`nx_get_canvas()`, and canvas sources are drawn through the
proper `SkSurface::makeImageSnapshot()` path (which keeps a stable SkImage
identity while the surface is unchanged, so the GPU texture cache stays
effective).
