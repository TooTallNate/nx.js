---
"@nx.js/runtime": patch
---

fix: `Gamepad.id` is now unique per controller (e.g. `"switch-gamepad-0"`) instead of an empty string for every pad. Previously all controllers shared an empty `id`, so the standard Web Gamepad pattern of keying per-controller state/config/slot-assignment by `gamepad.id` collapsed every connected pad onto a single entry. The id is derived from the controller's libnx `HidNpadIdType` index (the stable per-pad discriminator).
