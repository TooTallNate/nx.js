---
"@nx.js/runtime": minor
---

feat: implement the Web Bluetooth API (`navigator.bluetooth`) for BLE GATT client communication

`requestDevice()`, `BluetoothDevice`, `BluetoothRemoteGATTServer` /
`Service` / `Characteristic`, `BluetoothCharacteristicProperties`, and
`BluetoothUUID` — backed by the Switch's application-facing `btm.u` + `bt`
services. Supports service-UUID-based scanning, GATT discovery, reads,
writes (with/without response), notifications (with automatic CCCD writes),
browser-style ATT MTU negotiation, `gattserverdisconnected` events, and an
nx.js `deviceId` extension for connecting to devices by known address
(including devices that advertise no service UUIDs).

Verified end-to-end on real hardware by printing a label on a Niimbot D11
label printer using the same Web Bluetooth code paths as a Chrome web app.
