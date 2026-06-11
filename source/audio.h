#pragma once
#include "types.h"

struct nx_audio_graph;
struct nx_audio_sink;

// The native handle wrapped by AudioContext / OfflineAudioContext JS objects.
// Shared with video.cc (videoCreateAudioNode unwraps it to reach the graph).
typedef struct nx_audio_ctx {
	nx_audio_graph *graph;
	nx_audio_sink *sink; // NULL for offline contexts / after close()
} nx_audio_ctx_t;

void nx_init_audio(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
