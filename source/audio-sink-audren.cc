// Nintendo Switch audio output sink — streams the Web Audio graph to the
// audren service (via libnx's AudioDriver) using double-buffered wavebufs fed
// from a dedicated render thread.
//
// Process-global model: ONE audren session + AudioDriver + render thread is
// shared by all attached graphs (audrenInitialize can only be brought up once
// per process); each attached graph gets its own voice on the final mix. The
// render thread is the only code that touches the driver after init —
// attach/detach coordinate with it through `g.mutex`.
//
// This file is device-only (libnx); the host nxjs-test binary compiles
// packages/runtime/test/src/audio-sink.cc instead.
#include "audio-sink.h"
#include "audio-graph.h"
#include <malloc.h>
#include <string.h>
#include <switch.h>
#include <vector>

#define SINK_SAMPLE_RATE 48000
#define SINK_NUM_VOICES 24
#define SINK_BUF_FRAMES 1024 // per wavebuf (multiple of the render quantum)
#define SINK_NUM_BUFS 2
#define SINK_ALIGN 0x1000
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

struct nx_audio_sink {
	nx_audio_graph *graph;
	int voice_id;
	void *mempool;
	size_t mempool_size;
	int mempool_id;
	AudioDriverWaveBuf wavebufs[SINK_NUM_BUFS];
};

namespace {

const AudioRendererConfig ar_config = {
    .output_rate = AudioRendererOutputRate_48kHz,
    .num_voices = SINK_NUM_VOICES,
    .num_effects = 0,
    .num_sinks = 1,
    .num_mix_objs = 1,
    .num_mix_buffers = 2,
};

struct {
	Mutex mutex; // guards everything below + driver access from the thread
	bool initialized;
	AudioDriver drv;
	std::vector<nx_audio_sink *> sinks;
	bool voice_used[SINK_NUM_VOICES];
	Thread thread;
	bool thread_running;
	bool stop;
} g;

void render_thread(void *arg) {
	(void)arg;
	while (true) {
		mutexLock(&g.mutex);
		if (g.stop) {
			mutexUnlock(&g.mutex);
			break;
		}
		for (nx_audio_sink *sink : g.sinks) {
			for (int b = 0; b < SINK_NUM_BUFS; b++) {
				AudioDriverWaveBuf *wb = &sink->wavebufs[b];
				if (wb->state != AudioDriverWaveBufState_Free &&
				    wb->state != AudioDriverWaveBufState_Done)
					continue;
				int16_t *pcm = (int16_t *)wb->data_raw;
				nx_audio_graph_render_s16(sink->graph, pcm, SINK_BUF_FRAMES);
				armDCacheFlush(pcm, SINK_BUF_FRAMES * 2 * sizeof(int16_t));
				audrvVoiceAddWaveBuf(&g.drv, sink->voice_id, wb);
			}
			if (!audrvVoiceIsPlaying(&g.drv, sink->voice_id))
				audrvVoiceStart(&g.drv, sink->voice_id);
		}
		audrvUpdate(&g.drv);
		mutexUnlock(&g.mutex);
		audrenWaitFrame();
	}
}

// Bring up audren + the driver + the render thread. Caller holds g.mutex.
const char *global_init(void) {
	if (g.initialized)
		return NULL;
	Result rc = audrenInitialize(&ar_config);
	if (R_FAILED(rc))
		return "audrenInitialize() failed";
	rc = audrvCreate(&g.drv, &ar_config, 2);
	if (R_FAILED(rc)) {
		audrenExit();
		return "audrvCreate() failed";
	}
	static const u8 sink_channels[] = {0, 1};
	audrvDeviceSinkAdd(&g.drv, AUDREN_DEFAULT_DEVICE_NAME, 2, sink_channels);
	rc = audrenStartAudioRenderer();
	if (R_FAILED(rc)) {
		audrvClose(&g.drv);
		audrenExit();
		return "audrenStartAudioRenderer() failed";
	}
	audrvUpdate(&g.drv);
	memset(g.voice_used, 0, sizeof(g.voice_used));
	g.stop = false;
	// Higher priority than the main thread (0x2C) so mixing keeps up; any
	// core (-2). 64 KiB of stack is plenty for the graph render.
	rc = threadCreate(&g.thread, render_thread, NULL, NULL, 0x10000, 0x20, -2);
	if (R_FAILED(rc) || R_FAILED(threadStart(&g.thread))) {
		audrvClose(&g.drv);
		audrenExit();
		return "failed to start audio render thread";
	}
	g.thread_running = true;
	g.initialized = true;
	return NULL;
}

// Tear down audren when the last sink detaches. Caller holds g.mutex; the
// mutex is released while joining the thread.
void global_exit(void) {
	if (!g.initialized)
		return;
	g.stop = true;
	mutexUnlock(&g.mutex);
	threadWaitForExit(&g.thread);
	threadClose(&g.thread);
	mutexLock(&g.mutex);
	g.thread_running = false;
	audrvClose(&g.drv);
	audrenExit();
	g.initialized = false;
}

} // namespace

nx_audio_sink_t *nx_audio_sink_attach(nx_audio_graph *graph,
                                      const char **err) {
	mutexLock(&g.mutex);
	const char *init_err = global_init();
	if (init_err) {
		mutexUnlock(&g.mutex);
		*err = init_err;
		return NULL;
	}
	int voice_id = -1;
	for (int i = 0; i < SINK_NUM_VOICES; i++) {
		if (!g.voice_used[i]) {
			voice_id = i;
			break;
		}
	}
	if (voice_id < 0) {
		if (g.sinks.empty())
			global_exit();
		mutexUnlock(&g.mutex);
		*err = "no free audio voices";
		return NULL;
	}

	size_t mempool_size =
	    ALIGN_UP(SINK_NUM_BUFS * SINK_BUF_FRAMES * 2 * sizeof(int16_t),
	             SINK_ALIGN);
	void *mempool = memalign(SINK_ALIGN, mempool_size);
	if (!mempool) {
		if (g.sinks.empty())
			global_exit();
		mutexUnlock(&g.mutex);
		*err = "failed to allocate audio buffer";
		return NULL;
	}
	memset(mempool, 0, mempool_size);
	armDCacheFlush(mempool, mempool_size);

	nx_audio_sink *sink = new nx_audio_sink();
	sink->graph = graph;
	nx_audio_graph_ref(graph);
	sink->voice_id = voice_id;
	g.voice_used[voice_id] = true;
	sink->mempool = mempool;
	sink->mempool_size = mempool_size;
	sink->mempool_id = audrvMemPoolAdd(&g.drv, mempool, mempool_size);
	audrvMemPoolAttach(&g.drv, sink->mempool_id);

	// The voice plays at the graph's sample rate; audren resamples to 48 kHz.
	int channels = 2;
	audrvVoiceInit(&g.drv, voice_id, channels, PcmFormat_Int16,
	               (int)sink->graph->sample_rate);
	audrvVoiceSetDestinationMix(&g.drv, voice_id, AUDREN_FINAL_MIX_ID);
	audrvVoiceSetMixFactor(&g.drv, voice_id, 1.0f, 0, 0);
	audrvVoiceSetMixFactor(&g.drv, voice_id, 0.0f, 0, 1);
	audrvVoiceSetMixFactor(&g.drv, voice_id, 0.0f, 1, 0);
	audrvVoiceSetMixFactor(&g.drv, voice_id, 1.0f, 1, 1);

	memset(sink->wavebufs, 0, sizeof(sink->wavebufs));
	for (int b = 0; b < SINK_NUM_BUFS; b++) {
		AudioDriverWaveBuf *wb = &sink->wavebufs[b];
		wb->data_raw =
		    (uint8_t *)mempool + b * SINK_BUF_FRAMES * 2 * sizeof(int16_t);
		wb->size = SINK_BUF_FRAMES * 2 * sizeof(int16_t);
		wb->start_sample_offset = 0;
		wb->end_sample_offset = SINK_BUF_FRAMES;
		wb->state = AudioDriverWaveBufState_Free;
	}

	g.sinks.push_back(sink);
	mutexUnlock(&g.mutex);
	return sink;
}

void nx_audio_sink_detach(nx_audio_sink_t *sink) {
	mutexLock(&g.mutex);
	for (auto it = g.sinks.begin(); it != g.sinks.end(); ++it) {
		if (*it == sink) {
			g.sinks.erase(it);
			break;
		}
	}
	if (g.initialized) {
		audrvVoiceStop(&g.drv, sink->voice_id);
		audrvVoiceDrop(&g.drv, sink->voice_id);
		audrvUpdate(&g.drv);
		audrvMemPoolDetach(&g.drv, sink->mempool_id);
		audrvMemPoolRemove(&g.drv, sink->mempool_id);
	}
	g.voice_used[sink->voice_id] = false;
	if (g.sinks.empty())
		global_exit();
	mutexUnlock(&g.mutex);
	free(sink->mempool);
	nx_audio_graph_unref(sink->graph);
	delete sink;
}
