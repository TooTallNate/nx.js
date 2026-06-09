---
"@nx.js/runtime": minor
---

feat: `Gamepad.id` now reports the controller's real hardware identity. On firmware 5.0.0+ it is the device name plus the controller's serial number (e.g. `"Nintendo Switch Pro Controller (XAW10012345678)"`), queried from the `hid:sys` service. The serial is resolved lazily and cached — it is never read on the input-polling hot path, and is invalidated only when a controller connects or disconnects. When the serial is unavailable (older firmware, no serial, or a system error) `id` falls back to the previous unique-per-slot `"switch-gamepad-<index>"` string.

Additionally, the global `gamepadconnected` and `gamepaddisconnected` events now fire when controllers are connected to or disconnected from the system (previously the `GamepadEvent` type existed but was never dispatched).
