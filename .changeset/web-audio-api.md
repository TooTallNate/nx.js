---
"@nx.js/runtime": minor
---

feat: implement the Web Audio API (`AudioContext`, `OfflineAudioContext`, `AudioBuffer`, `AudioBufferSourceNode`, `GainNode`, `StereoPannerNode`, `AudioParam` automation, `decodeAudioData()`)

Audio is now processed by a native graph engine on a dedicated real-time render
thread (128-frame quanta, like browsers), streamed to the Switch `audren`
service via double-buffered wavebufs. The `Audio` element is re-implemented on
top of the new engine.
