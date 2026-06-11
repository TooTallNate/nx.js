---
"@nx.js/runtime": minor
---

feat: add the `Video` element for video playback, backed by a new FFmpeg media pipeline

`new Video()` provides an `HTMLVideoElement`-like API (since nx.js has no DOM,
the app explicitly draws each frame via `ctx.drawImage(video, …)`, like
rendering a `<video>` onto a `<canvas>`). Supports any container/codec FFmpeg
software-decodes (WebM VP8/VP9, MP4/MKV H.264/H.265, AV1, …), with audio
tracks played through the Web Audio engine and A/V sync slaved to the audio
clock. Includes keyframe seeking, gapless looping, and
`getVideoPlaybackQuality()`.

`decodeAudioData()` and the `Audio` element are now also backed by FFmpeg
(replacing the vendored dr_mp3/dr_wav/stb_vorbis decoders), so every
FFmpeg-supported audio format works: MP3, WAV, OGG Vorbis, Opus, FLAC,
AAC/M4A, and more.
