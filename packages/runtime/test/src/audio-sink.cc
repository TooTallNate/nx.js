// Host (nxjs-test) audio output sink — a wall-clock-paced consumer thread.
//
// There is no audible output on the host; the sink exists so that an online
// AudioContext behaves like the device: currentTime advances in real time,
// scheduled sources progress/finish, and the graph render code (the exact
// same audio-graph.cc the device runs) is exercised under threading.
#include "audio-sink.h"
#include "audio-graph.h"
#include <atomic>
#include <chrono>
#include <thread>

struct nx_audio_sink {
	nx_audio_graph *graph;
	std::thread thread;
	std::atomic<bool> stop{false};
};

namespace {

constexpr uint32_t CHUNK_FRAMES = 1024; // multiple of the render quantum

void sink_thread(nx_audio_sink *s) {
	using clock = std::chrono::steady_clock;
	int16_t scratch[CHUNK_FRAMES * 2];
	const double rate = s->graph->sample_rate; // immutable after create
	const auto start = clock::now();
	uint64_t consumed = 0;
	while (!s->stop.load(std::memory_order_relaxed)) {
		double elapsed =
		    std::chrono::duration<double>(clock::now() - start).count();
		uint64_t target = (uint64_t)(elapsed * rate);
		while (consumed + CHUNK_FRAMES <= target &&
		       !s->stop.load(std::memory_order_relaxed)) {
			nx_audio_graph_render_s16(s->graph, scratch, CHUNK_FRAMES);
			consumed += CHUNK_FRAMES;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

} // namespace

nx_audio_sink_t *nx_audio_sink_attach(nx_audio_graph *graph,
                                      const char **err) {
	(void)err;
	nx_audio_sink *sink = new nx_audio_sink();
	sink->graph = graph;
	nx_audio_graph_ref(graph);
	sink->thread = std::thread(sink_thread, sink);
	return sink;
}

void nx_audio_sink_detach(nx_audio_sink_t *sink) {
	sink->stop.store(true, std::memory_order_relaxed);
	sink->thread.join();
	nx_audio_graph_unref(sink->graph);
	delete sink;
}
