#pragma once
// Portable media (video + audio) decode pipeline over ffmpeg.
//
// Compiled into BOTH the device runtime (switch-ffmpeg 7.x) and the host
// nxjs-test binary (distro ffmpeg 5.x+) — no V8, no libnx. Each open media
// owns a dedicated decode thread which demuxes the container, decodes the
// video stream into a small ring of presentation-ready BGRA frames (PTS
// stamped), and decodes/resamples the audio stream into an audio-graph
// stream-source node (see NX_AUDIO_NODE_STREAM_SOURCE in audio-graph.h).
//
// Threading contract:
//   - All functions below are called from the main (JS loop) thread, except
//     nx_media_open which may run on a libuv worker.
//   - The decode thread is internal; commands (play/pause/seek/loop/quit)
//     are delivered via atomics + a condition variable.
//   - nx_media_present() swaps the newest due frame into the caller's BGRA
//     buffer (pointer swap, zero copy) based on the media clock. The clock
//     is slaved to the audio stream node's consumed-frame counter when an
//     audio track is playing, and to the monotonic wall clock otherwise.
#include <memory>
#include <stddef.h>
#include <stdint.h>

struct nx_audio_node;

typedef struct nx_media nx_media_t;

// Channel cap for whole-file audio decoding (Web Audio allows 32).
#define NX_MEDIA_MAX_CHANNELS 32

// Decode an entire audio resource (any ffmpeg-supported container/codec)
// from memory into planar f32 channel buffers at the file's native sample
// rate. Backs `decodeAudioData()` and the `Audio` element. On success fills
// `channels[0..num_channels)` with malloc'd buffers (caller frees), `length`
// (frames) and `sample_rate`, and returns true. Blocking — call on a worker
// thread. On failure fills `errbuf` and returns false.
bool nx_media_decode_audio(const uint8_t *data, size_t size,
                           float *channels[NX_MEDIA_MAX_CHANNELS],
                           int *num_channels, uint32_t *length,
                           uint32_t *sample_rate, char *errbuf,
                           size_t errbuf_size);

// Open a media resource and probe its streams. Exactly one of `path` or
// `mem` must be provided; for `mem`, `keepalive` must own the buffer (e.g. a
// shared_ptr<v8::BackingStore>) — the media holds it until
// nx_media_destroy(), since the decode thread streams from it for the
// media's whole lifetime. Blocking — call off the main thread. Returns NULL
// and fills `errbuf` on failure.
nx_media_t *nx_media_open(const char *path, const uint8_t *mem,
                          size_t mem_size, std::shared_ptr<void> keepalive,
                          char *errbuf, size_t errbuf_size);

// Metadata (valid after a successful open).
int nx_media_width(nx_media_t *m);
int nx_media_height(nx_media_t *m);
double nx_media_duration(nx_media_t *m); // seconds (0 if unknown)
bool nx_media_has_audio(nx_media_t *m);
bool nx_media_has_video(nx_media_t *m);

// Attach the audio output. `node` must be an NX_AUDIO_NODE_STREAM_SOURCE
// whose graph runs at `sample_rate`; the decoder resamples the audio track
// to interleaved stereo f32 at that rate. The caller owns the node and must
// keep it alive until nx_media_destroy(). Call before nx_media_play().
void nx_media_set_audio_node(nx_media_t *m, nx_audio_node *node,
                             double sample_rate);

// Transport controls (non-blocking; the decode thread reacts).
void nx_media_play(nx_media_t *m);
void nx_media_pause(nx_media_t *m);
void nx_media_seek(nx_media_t *m, double seconds);
void nx_media_set_loop(nx_media_t *m, bool loop);

// Presentation: if a video frame is due at the current media clock, swap it
// into `*buffer_inout` (a caller-owned width*height*4 BGRA buffer; the
// pointer is exchanged with the ring slot's buffer). Returns true if a new
// frame was presented. Call from the main thread (e.g. once per host frame).
bool nx_media_present(nx_media_t *m, uint8_t **buffer_inout);

// Current playback position in media seconds (wraps when looping).
double nx_media_current_time(nx_media_t *m);

// Number of decoded-and-waiting video frames (readiness heuristic).
uint32_t nx_media_buffered_frames(nx_media_t *m);

// Presentation quality counters (getVideoPlaybackQuality): frames actually
// presented, and frames skipped because a newer frame was already due.
uint64_t nx_media_presented_frames(nx_media_t *m);
uint64_t nx_media_dropped_frames(nx_media_t *m);

// True once playback reached the end of the stream (never true while
// looping) and all buffered frames/audio have been presented/consumed.
bool nx_media_ended(nx_media_t *m);

// True while a seek is in flight (target not yet decoded).
bool nx_media_seeking(nx_media_t *m);

// Sticky fatal decode error message, or NULL.
const char *nx_media_error(nx_media_t *m);

// Stop the decode thread (joins it) and free everything. The audio node is
// NOT freed (caller-owned).
void nx_media_destroy(nx_media_t *m);
