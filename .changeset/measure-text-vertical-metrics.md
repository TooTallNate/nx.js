---
'@nx.js/runtime': patch
---

fix: `measureText()` now reports the vertical `TextMetrics` fields instead of zero. `actualBoundingBox{Ascent,Descent,Left,Right}` come from the shaped glyphs' Skia bounds, `fontBoundingBox{Ascent,Descent}` and `emHeight{Ascent,Descent}` from the font's own metrics, and the three baseline offsets follow from those. Every field respects `textBaseline`, sharing one `baseline_offset()` helper with `layout_glyphs()` so measurement and rendering cannot disagree.
