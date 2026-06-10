#pragma once
// Platform audio output sink for online AudioContexts.
//
// The sink owns a real-time render thread that periodically pulls PCM from the
// audio graph (nx_audio_graph_render_s16) and submits it to the platform audio
// output. Two implementations exist:
//   - source/audio-sink-audren.cc — Nintendo Switch (libnx audren streaming
//     wavebufs). Compiled into the device runtime via the Makefile glob.
//   - packages/runtime/test/src/audio-sink.cc — host nxjs-test (wall-clock
//     paced consumer thread; output is discarded).
#include <stddef.h>

struct nx_audio_graph;
typedef struct nx_audio_sink nx_audio_sink_t;

// Attach `graph` to the platform audio output (takes a graph ref for the
// lifetime of the sink). Returns NULL and sets `*err` to a static string on
// failure.
nx_audio_sink_t *nx_audio_sink_attach(nx_audio_graph *graph, const char **err);

// Stop feeding the graph to the output and release the sink (drops the graph
// ref). Idempotent per sink pointer is NOT required — call exactly once.
void nx_audio_sink_detach(nx_audio_sink_t *sink);
