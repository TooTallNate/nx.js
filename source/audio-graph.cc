// Portable Web Audio graph engine implementation. See audio-graph.h for the
// architecture overview. No V8 / libnx / libuv — compiled into both the device
// runtime and the host nxjs-test binary.
#include "audio-graph.h"
#include <algorithm>
#include <string.h>

namespace {

constexpr int Q = NX_AUDIO_RENDER_QUANTUM;

float clampf(float v, float lo, float hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// ---------------------------------------------------------------------------
// AudioParam timeline evaluation
// ---------------------------------------------------------------------------

// Computes the parameter value at time `t` by walking the (time-sorted) event
// list. This is a pragmatic implementation of the Web Audio "computedValue"
// algorithm covering the common shapes: set, linear/exponential ramps,
// setTarget decay, and value curves. Ramps anchor at the previous event's
// (time, value); a setTarget remains in effect until the next event.
float param_value_at(const nx_audio_param *p, double t) {
	const auto &evs = p->events;
	double v_prev = p->value;
	double t_prev = 0;
	for (size_t i = 0; i < evs.size(); i++) {
		const nx_audio_param_event &e = evs[i];
		if (e.time > t) {
			// `t` falls before this event takes effect. Ramps interpolate
			// from the previous anchor toward this (future) event.
			if (e.type == NX_AUDIO_PARAM_LINEAR_RAMP) {
				if (e.time <= t_prev)
					return clampf((float)e.value, p->min_value, p->max_value);
				double f = (t - t_prev) / (e.time - t_prev);
				if (f < 0)
					f = 0;
				return clampf((float)(v_prev + (e.value - v_prev) * f),
				              p->min_value, p->max_value);
			}
			if (e.type == NX_AUDIO_PARAM_EXPONENTIAL_RAMP) {
				if (v_prev == 0 || (v_prev < 0) != (e.value < 0) ||
				    e.time <= t_prev)
					return clampf((float)v_prev, p->min_value, p->max_value);
				double f = (t - t_prev) / (e.time - t_prev);
				if (f < 0)
					f = 0;
				return clampf((float)(v_prev * pow(e.value / v_prev, f)),
				              p->min_value, p->max_value);
			}
			// Other event types have no effect before their start time.
			return clampf((float)v_prev, p->min_value, p->max_value);
		}
		// Event time <= t: apply it and move the anchor forward.
		switch (e.type) {
		case NX_AUDIO_PARAM_SET_VALUE:
		case NX_AUDIO_PARAM_LINEAR_RAMP:
		case NX_AUDIO_PARAM_EXPONENTIAL_RAMP:
			v_prev = e.value;
			t_prev = e.time;
			break;
		case NX_AUDIO_PARAM_SET_VALUE_CURVE: {
			size_t n = e.curve.size();
			if (n == 0)
				break;
			double tend = e.time + e.duration;
			if (t < tend && e.duration > 0) {
				double f = (t - e.time) / e.duration * (double)(n - 1);
				size_t k = (size_t)f;
				if (k >= n - 1)
					return clampf(e.curve[n - 1], p->min_value, p->max_value);
				double frac = f - (double)k;
				return clampf(
				    (float)(e.curve[k] + (e.curve[k + 1] - e.curve[k]) * frac),
				    p->min_value, p->max_value);
			}
			v_prev = e.curve[n - 1];
			t_prev = tend;
			break;
		}
		case NX_AUDIO_PARAM_SET_TARGET: {
			// In effect until the next event (or `t`, whichever is sooner).
			// An event at exactly `t` takes over (events apply at their
			// start time, inclusive), so the comparison is `<=`.
			double tstop = t;
			bool next_applies =
			    i + 1 < evs.size() && evs[i + 1].time <= t;
			if (next_applies)
				tstop = evs[i + 1].time;
			double val;
			if (e.time_constant <= 0) {
				val = e.value;
			} else {
				val = e.value + (v_prev - e.value) *
				                    exp(-(tstop - e.time) / e.time_constant);
			}
			if (!next_applies)
				return clampf((float)val, p->min_value, p->max_value);
			v_prev = val;
			t_prev = tstop;
			break;
		}
		}
	}
	return clampf((float)v_prev, p->min_value, p->max_value);
}

// Fill `out[0..n)` with a-rate values for frame times t0, t0+1/sr, ...
void param_fill(const nx_audio_param *p, double t0, double inv_sr, float *out,
                int n) {
	if (p->events.empty()) {
		float v = clampf(p->value, p->min_value, p->max_value);
		for (int i = 0; i < n; i++)
			out[i] = v;
		return;
	}
	for (int i = 0; i < n; i++)
		out[i] = param_value_at(p, t0 + i * inv_sr);
}

// Insert an event keeping the list time-sorted (stable for equal times). A
// SET_VALUE at the exact time of an existing SET_VALUE replaces it.
void param_insert_event(nx_audio_param *p, nx_audio_param_event &&e) {
	if (e.type == NX_AUDIO_PARAM_SET_VALUE) {
		for (auto &existing : p->events) {
			if (existing.time == e.time &&
			    existing.type == NX_AUDIO_PARAM_SET_VALUE) {
				existing = std::move(e);
				return;
			}
		}
	}
	auto it = std::upper_bound(
	    p->events.begin(), p->events.end(), e.time,
	    [](double t, const nx_audio_param_event &ev) { return t < ev.time; });
	p->events.insert(it, std::move(e));
}

// ---------------------------------------------------------------------------
// Node processing
// ---------------------------------------------------------------------------

void zero_bus(nx_audio_node *n) {
	memset(n->bus, 0, sizeof(n->bus));
}

void process_node(nx_audio_graph *g, nx_audio_node *n, double t0);

// `out_channels` is set to the computed channel count of the summed input.
void sum_inputs(nx_audio_graph *g, nx_audio_node *n, double t0,
                float in[NX_AUDIO_CHANNELS][Q], int *out_channels) {
	memset(in, 0, sizeof(float) * NX_AUDIO_CHANNELS * Q);
	int ch = 1;
	for (nx_audio_node *src : n->inputs) {
		process_node(g, src, t0);
		for (int c = 0; c < NX_AUDIO_CHANNELS; c++)
			for (int i = 0; i < Q; i++)
				in[c][i] += src->bus[c][i];
		if (src->bus_ch == 2)
			ch = 2;
	}
	*out_channels = ch;
}

void process_buffer_source(nx_audio_graph *g, nx_audio_node *n, double t0) {
	zero_bus(n);
	n->bus_ch = 1;
	if (!n->started || n->playback_state == NX_AUDIO_SOURCE_FINISHED)
		return;

	bool has_buf = !n->buffer_channels.empty() && n->buffer_length > 0 &&
	               n->buffer_sample_rate > 0;
	if (has_buf && n->buffer_channels.size() > 1)
		n->bus_ch = 2;

	// k-rate playback rate, computed once per quantum.
	double rate = param_value_at(&n->params[0], t0);
	double detune = param_value_at(&n->params[1], t0);
	double r = rate * exp2(detune / 1200.0);
	if (has_buf)
		r *= n->buffer_sample_rate / g->sample_rate;

	double inv_sr = 1.0 / g->sample_rate;
	double buf_len = has_buf ? (double)n->buffer_length : 0;

	// Loop points, in buffer frames.
	double loop_s = 0, loop_e = buf_len;
	if (n->loop && has_buf) {
		loop_s = n->loop_start * n->buffer_sample_rate;
		loop_e = n->loop_end > 0 ? n->loop_end * n->buffer_sample_rate
		                         : buf_len;
		if (loop_s < 0)
			loop_s = 0;
		if (loop_e > buf_len)
			loop_e = buf_len;
		if (loop_e <= loop_s) {
			loop_s = 0;
			loop_e = buf_len;
		}
	}
	double dur_frames =
	    n->duration >= 0 && has_buf ? n->duration * n->buffer_sample_rate : -1;

	const float *ch0 = has_buf ? n->buffer_channels[0] : nullptr;
	const float *ch1 = has_buf && n->buffer_channels.size() > 1
	                       ? n->buffer_channels[1]
	                       : ch0;

	bool finished = false;
	for (int i = 0; i < Q; i++) {
		double t = t0 + i * inv_sr;
		if (n->stop_time >= 0 && t >= n->stop_time) {
			finished = true;
			break;
		}
		if (t < n->start_time)
			continue; // bus is already zero
		if (!n->playing) {
			n->playing = true;
			n->position = n->start_offset * n->buffer_sample_rate;
			if (n->position < 0)
				n->position = 0;
			if (n->loop && n->position > loop_e)
				n->position = loop_s;
			n->played_frames = 0;
		}
		if (!has_buf)
			continue; // null buffer: silence until stop()
		if (dur_frames >= 0 && n->played_frames >= dur_frames) {
			finished = true;
			break;
		}
		if (!n->loop && (n->position >= buf_len || n->position < 0)) {
			finished = true;
			break;
		}
		uint32_t i0 = (uint32_t)n->position;
		if (i0 >= n->buffer_length)
			i0 = n->buffer_length - 1;
		uint32_t i1 = i0 + 1 < n->buffer_length ? i0 + 1 : i0;
		double frac = n->position - (double)i0;
		n->bus[0][i] = (float)(ch0[i0] + (ch0[i1] - ch0[i0]) * frac);
		n->bus[1][i] = (float)(ch1[i0] + (ch1[i1] - ch1[i0]) * frac);
		n->position += r;
		n->played_frames += fabs(r);
		if (n->loop) {
			double ll = loop_e - loop_s;
			if (ll > 0) {
				while (n->position >= loop_e)
					n->position -= ll;
				while (n->position < loop_s && r < 0)
					n->position += ll;
			}
		}
	}
	if (finished) {
		n->playback_state = NX_AUDIO_SOURCE_FINISHED;
		n->playing = false;
	}
}

void process_stream_source(nx_audio_graph *g, nx_audio_node *n) {
	zero_bus(n);
	n->bus_ch = 2;
	if (!n->stream_ring || !n->stream_playing.load(std::memory_order_relaxed))
		return;
	uint64_t read = n->stream_read_pos.load(std::memory_order_relaxed);
	uint64_t write = n->stream_write_pos.load(std::memory_order_acquire);
	uint32_t avail = (uint32_t)(write - read);
	uint32_t frames = avail < (uint32_t)Q ? avail : (uint32_t)Q;
	const float *ring = n->stream_ring.get();
	for (uint32_t i = 0; i < frames; i++) {
		uint32_t idx = (uint32_t)((read + i) % n->stream_capacity);
		n->bus[0][i] = ring[idx * 2];
		n->bus[1][i] = ring[idx * 2 + 1];
	}
	// Underrun: the remainder of the bus stays silent and is NOT counted as
	// consumed, so the media clock only advances for real audio.
	n->stream_read_pos.store(read + frames, std::memory_order_release);
	(void)g;
}

void process_gain(nx_audio_graph *g, nx_audio_node *n, double t0) {
	float in[NX_AUDIO_CHANNELS][Q];
	int ch;
	sum_inputs(g, n, t0, in, &ch);
	n->bus_ch = ch;
	float gain[Q];
	param_fill(&n->params[0], t0, 1.0 / g->sample_rate, gain, Q);
	for (int i = 0; i < Q; i++) {
		n->bus[0][i] = in[0][i] * gain[i];
		n->bus[1][i] = in[1][i] * gain[i];
	}
}

void process_stereo_panner(nx_audio_graph *g, nx_audio_node *n, double t0) {
	float in[NX_AUDIO_CHANNELS][Q];
	int ch;
	sum_inputs(g, n, t0, in, &ch);
	n->bus_ch = 2; // panner output is always stereo
	float pan[Q];
	param_fill(&n->params[0], t0, 1.0 / g->sample_rate, pan, Q);
	constexpr double HALF_PI = 1.57079632679489661923;
	if (ch == 1) {
		// Mono input (L==R): x = (pan + 1) / 2.
		for (int i = 0; i < Q; i++) {
			double x = (pan[i] + 1) / 2;
			n->bus[0][i] = (float)(in[0][i] * cos(x * HALF_PI));
			n->bus[1][i] = (float)(in[0][i] * sin(x * HALF_PI));
		}
	} else {
		// Stereo input, per the spec's equal-power algorithm.
		for (int i = 0; i < Q; i++) {
			double p = pan[i];
			double x = p <= 0 ? p + 1 : p;
			float gl = (float)cos(x * HALF_PI);
			float gr = (float)sin(x * HALF_PI);
			if (p <= 0) {
				n->bus[0][i] = in[0][i] + in[1][i] * gl;
				n->bus[1][i] = in[1][i] * gr;
			} else {
				n->bus[0][i] = in[0][i] * gl;
				n->bus[1][i] = in[1][i] + in[0][i] * gr;
			}
		}
	}
}

void process_destination(nx_audio_graph *g, nx_audio_node *n, double t0) {
	float in[NX_AUDIO_CHANNELS][Q];
	int ch;
	sum_inputs(g, n, t0, in, &ch);
	n->bus_ch = ch;
	memcpy(n->bus, in, sizeof(in));
}

void process_node(nx_audio_graph *g, nx_audio_node *n, double t0) {
	if (n->processed_quantum == g->quantum_id)
		return; // already rendered this quantum (fan-out memoization)
	if (n->processing) {
		// Cycle (not legal in Web Audio without a DelayNode, which we don't
		// implement) — break it with silence.
		zero_bus(n);
		return;
	}
	n->processing = true;
	switch (n->type) {
	case NX_AUDIO_NODE_BUFFER_SOURCE:
		process_buffer_source(g, n, t0);
		break;
	case NX_AUDIO_NODE_STREAM_SOURCE:
		process_stream_source(g, n);
		break;
	case NX_AUDIO_NODE_GAIN:
		process_gain(g, n, t0);
		break;
	case NX_AUDIO_NODE_STEREO_PANNER:
		process_stereo_panner(g, n, t0);
		break;
	case NX_AUDIO_NODE_DESTINATION:
		process_destination(g, n, t0);
		break;
	}
	n->processing = false;
	n->processed_quantum = g->quantum_id;
}

// Renders one 128-frame quantum into the destination bus and advances time.
// Caller holds the graph mutex.
void render_quantum(nx_audio_graph *g) {
	g->quantum_id++;
	double t0 = (double)g->frames_rendered / g->sample_rate;
	process_node(g, g->destination, t0);
	// Sources not reachable from the destination still progress through their
	// schedule (so `ended` fires even for unconnected/indirect sources).
	for (nx_audio_node *n : g->nodes) {
		if (n->type == NX_AUDIO_NODE_BUFFER_SOURCE &&
		    n->processed_quantum != g->quantum_id)
			process_node(g, n, t0);
	}
	g->frames_rendered += Q;
}

void graph_destroy(nx_audio_graph *g) {
	for (nx_audio_node *n : g->nodes)
		delete n;
	delete g;
}

nx_audio_node *node_new(nx_audio_graph *g, nx_audio_node_type type) {
	nx_audio_node *n = new nx_audio_node();
	n->graph = g;
	n->type = type;
	zero_bus(n);
	switch (type) {
	case NX_AUDIO_NODE_GAIN: {
		nx_audio_param gain;
		gain.value = 1.f;
		n->params.push_back(gain);
		break;
	}
	case NX_AUDIO_NODE_STEREO_PANNER: {
		nx_audio_param pan;
		pan.value = 0.f;
		pan.min_value = -1.f;
		pan.max_value = 1.f;
		n->params.push_back(pan);
		break;
	}
	case NX_AUDIO_NODE_BUFFER_SOURCE: {
		nx_audio_param rate;
		rate.value = 1.f;
		n->params.push_back(rate);
		nx_audio_param detune;
		detune.value = 0.f;
		n->params.push_back(detune);
		break;
	}
	case NX_AUDIO_NODE_STREAM_SOURCE: {
		// One second of buffering at the graph rate.
		n->stream_capacity = (uint32_t)g->sample_rate;
		n->stream_ring = std::make_unique<float[]>(
		    (size_t)n->stream_capacity * NX_AUDIO_CHANNELS);
		break;
	}
	case NX_AUDIO_NODE_DESTINATION:
		break;
	}
	g->nodes.push_back(n);
	return n;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API (all functions lock the graph mutex)
// ---------------------------------------------------------------------------

nx_audio_graph *nx_audio_graph_create(double sample_rate) {
	nx_audio_graph *g = new nx_audio_graph();
	g->sample_rate = sample_rate;
	g->destination = node_new(g, NX_AUDIO_NODE_DESTINATION);
	return g;
}

void nx_audio_graph_ref(nx_audio_graph *g) { g->refs.fetch_add(1); }

void nx_audio_graph_unref(nx_audio_graph *g) {
	if (g->refs.fetch_sub(1) == 1)
		graph_destroy(g);
}

double nx_audio_graph_current_time(nx_audio_graph *g) {
	std::lock_guard<std::mutex> lock(g->mutex);
	return (double)g->frames_rendered / g->sample_rate;
}

void nx_audio_graph_set_suspended(nx_audio_graph *g, bool suspended) {
	std::lock_guard<std::mutex> lock(g->mutex);
	g->suspended = suspended;
}

nx_audio_node *nx_audio_node_create(nx_audio_graph *g,
                                    nx_audio_node_type type) {
	nx_audio_graph_ref(g);
	std::lock_guard<std::mutex> lock(g->mutex);
	return node_new(g, type);
}

void nx_audio_node_release(nx_audio_node *n) {
	nx_audio_graph *g = n->graph;
	{
		std::lock_guard<std::mutex> lock(g->mutex);
		if (n->type != NX_AUDIO_NODE_DESTINATION) {
			for (nx_audio_node *src : n->inputs)
				src->outputs.erase(std::remove(src->outputs.begin(),
				                               src->outputs.end(), n),
				                   src->outputs.end());
			for (nx_audio_node *dst : n->outputs)
				dst->inputs.erase(
				    std::remove(dst->inputs.begin(), dst->inputs.end(), n),
				    dst->inputs.end());
			g->nodes.erase(std::remove(g->nodes.begin(), g->nodes.end(), n),
			               g->nodes.end());
			delete n;
		}
		// The destination node is graph-owned; only the ref is dropped.
	}
	nx_audio_graph_unref(g);
}

void nx_audio_node_connect(nx_audio_node *src, nx_audio_node *dst) {
	std::lock_guard<std::mutex> lock(src->graph->mutex);
	// Idempotent: multiple connections between the same nodes collapse.
	if (std::find(dst->inputs.begin(), dst->inputs.end(), src) !=
	    dst->inputs.end())
		return;
	dst->inputs.push_back(src);
	src->outputs.push_back(dst);
}

void nx_audio_node_disconnect(nx_audio_node *src, nx_audio_node *dst) {
	std::lock_guard<std::mutex> lock(src->graph->mutex);
	if (dst) {
		dst->inputs.erase(
		    std::remove(dst->inputs.begin(), dst->inputs.end(), src),
		    dst->inputs.end());
		src->outputs.erase(
		    std::remove(src->outputs.begin(), src->outputs.end(), dst),
		    src->outputs.end());
	} else {
		for (nx_audio_node *d : src->outputs)
			d->inputs.erase(
			    std::remove(d->inputs.begin(), d->inputs.end(), src),
			    d->inputs.end());
		src->outputs.clear();
	}
}

nx_audio_param *nx_audio_node_param(nx_audio_node *n, int index) {
	if (index < 0 || (size_t)index >= n->params.size())
		return nullptr;
	return &n->params[index];
}

float nx_audio_param_value(nx_audio_node *n, nx_audio_param *p) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	double t = (double)n->graph->frames_rendered / n->graph->sample_rate;
	return param_value_at(p, t);
}

void nx_audio_param_set_value(nx_audio_node *n, nx_audio_param *p,
                              float value) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	p->value = value;
	if (!p->events.empty()) {
		// Per spec, setting .value with scheduled automation behaves like
		// setValueAtTime(value, currentTime).
		double t = (double)n->graph->frames_rendered / n->graph->sample_rate;
		nx_audio_param_event e = {};
		e.type = NX_AUDIO_PARAM_SET_VALUE;
		e.time = t;
		e.value = value;
		param_insert_event(p, std::move(e));
	}
}

void nx_audio_param_schedule(nx_audio_node *n, nx_audio_param *p,
                             nx_audio_param_event_type type, double time,
                             float value, double time_constant) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	nx_audio_param_event e = {};
	e.type = type;
	e.time = time;
	e.value = value;
	e.time_constant = time_constant;
	param_insert_event(p, std::move(e));
}

void nx_audio_param_set_value_curve(nx_audio_node *n, nx_audio_param *p,
                                    const float *curve, size_t len,
                                    double start_time, double duration) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	nx_audio_param_event e = {};
	e.type = NX_AUDIO_PARAM_SET_VALUE_CURVE;
	e.time = start_time;
	e.duration = duration;
	e.curve.assign(curve, curve + len);
	param_insert_event(p, std::move(e));
}

void nx_audio_param_cancel(nx_audio_node *n, nx_audio_param *p, double time) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	p->events.erase(std::remove_if(p->events.begin(), p->events.end(),
	                               [time](const nx_audio_param_event &e) {
		                               return e.time >= time;
	                               }),
	                p->events.end());
}

void nx_audio_source_set_buffer(nx_audio_node *n, const float *const *channels,
                                int num_channels, uint32_t length,
                                double sample_rate,
                                std::vector<std::shared_ptr<void>> holds) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	n->buffer_channels.assign(channels, channels + num_channels);
	n->buffer_holds = std::move(holds);
	n->buffer_length = length;
	n->buffer_sample_rate = sample_rate;
}

void nx_audio_source_set_loop(nx_audio_node *n, bool loop, double loop_start,
                              double loop_end) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	n->loop = loop;
	n->loop_start = loop_start;
	n->loop_end = loop_end;
}

void nx_audio_source_start(nx_audio_node *n, double when, double offset,
                           double duration) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	if (n->started)
		return; // JS throws InvalidStateError before reaching here
	n->started = true;
	n->playback_state = NX_AUDIO_SOURCE_SCHEDULED;
	n->start_time = when;
	n->start_offset = offset;
	n->duration = duration;
}

void nx_audio_source_stop(nx_audio_node *n, double when) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	double t = (double)n->graph->frames_rendered / n->graph->sample_rate;
	if (when < t)
		when = t;
	n->stop_time = when;
}

int nx_audio_source_playback_state(nx_audio_node *n) {
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	return n->playback_state;
}

uint32_t nx_audio_stream_writable(nx_audio_node *n) {
	if (!n->stream_ring)
		return 0;
	uint64_t read = n->stream_read_pos.load(std::memory_order_acquire);
	uint64_t write = n->stream_write_pos.load(std::memory_order_relaxed);
	return n->stream_capacity - (uint32_t)(write - read);
}

uint32_t nx_audio_stream_write(nx_audio_node *n, const float *interleaved,
                               uint32_t frames) {
	if (!n->stream_ring)
		return 0;
	uint64_t read = n->stream_read_pos.load(std::memory_order_acquire);
	uint64_t write = n->stream_write_pos.load(std::memory_order_relaxed);
	uint32_t writable = n->stream_capacity - (uint32_t)(write - read);
	if (frames > writable)
		frames = writable;
	float *ring = n->stream_ring.get();
	for (uint32_t i = 0; i < frames; i++) {
		uint32_t idx = (uint32_t)((write + i) % n->stream_capacity);
		ring[idx * 2] = interleaved[i * 2];
		ring[idx * 2 + 1] = interleaved[i * 2 + 1];
	}
	n->stream_write_pos.store(write + frames, std::memory_order_release);
	return frames;
}

void nx_audio_stream_set_playing(nx_audio_node *n, bool playing) {
	n->stream_playing.store(playing, std::memory_order_relaxed);
}

uint64_t nx_audio_stream_consumed(nx_audio_node *n) {
	return n->stream_read_pos.load(std::memory_order_acquire);
}

void nx_audio_stream_flush(nx_audio_node *n) {
	// Producer is parked (decoder contract); empty the ring by advancing the
	// read position to the write position. Take the graph mutex so this
	// cannot interleave with a render quantum's read-modify-write.
	std::lock_guard<std::mutex> lock(n->graph->mutex);
	n->stream_read_pos.store(
	    n->stream_write_pos.load(std::memory_order_relaxed),
	    std::memory_order_release);
}

void nx_audio_graph_render_s16(nx_audio_graph *g, int16_t *out,
                               uint32_t frames) {
	std::lock_guard<std::mutex> lock(g->mutex);
	if (g->suspended || g->closed) {
		memset(out, 0, (size_t)frames * NX_AUDIO_CHANNELS * sizeof(int16_t));
		return;
	}
	uint32_t done = 0;
	while (done < frames) {
		render_quantum(g);
		uint32_t n = frames - done < Q ? frames - done : Q;
		const float *l = g->destination->bus[0];
		const float *r = g->destination->bus[1];
		for (uint32_t i = 0; i < n; i++) {
			float fl = clampf(l[i], -1.f, 1.f);
			float fr = clampf(r[i], -1.f, 1.f);
			out[(done + i) * 2] = (int16_t)lrintf(fl * 32767.f);
			out[(done + i) * 2 + 1] = (int16_t)lrintf(fr * 32767.f);
		}
		done += n;
	}
}

void nx_audio_graph_render_offline(nx_audio_graph *g, float *const *channels,
                                   int num_channels, uint32_t length) {
	std::lock_guard<std::mutex> lock(g->mutex);
	uint32_t done = 0;
	while (done < length) {
		render_quantum(g);
		uint32_t n = length - done < Q ? length - done : Q;
		const float *l = g->destination->bus[0];
		const float *r = g->destination->bus[1];
		if (num_channels == 1) {
			// Mono destination: speakers downmix = 0.5 * (L + R).
			for (uint32_t i = 0; i < n; i++)
				channels[0][done + i] = 0.5f * (l[i] + r[i]);
		} else {
			for (uint32_t i = 0; i < n; i++) {
				channels[0][done + i] = l[i];
				channels[1][done + i] = r[i];
			}
		}
		done += n;
	}
}
