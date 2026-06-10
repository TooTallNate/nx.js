// Web Audio API native bindings.
//
// This module is PORTABLE (compiled into both the device runtime and the host
// nxjs-test binary): the graph processing lives in audio-graph.cc and the
// platform output behind audio-sink.h (audren on device, a paced thread on
// host). Decoding (decodeAudioData) uses the vendored dr_mp3 / dr_wav /
// stb_vorbis single-header libraries on the libuv threadpool.
//
// JS object model: AudioContext/OfflineAudioContext wrap an nx_audio_ctx_t
// (graph + optional sink); every AudioNode wraps an nx_audio_node* directly.
// AudioParams are addressed as (node, param index) pairs — they have no native
// wrapper (the JS AudioParam object retains its AudioNode, which keeps the
// native node alive).
#include "audio.h"
#include "async.h"
#include "audio-graph.h"
#include "audio-sink.h"
#include "error.h"
#include "util.h"
#include "wrap.h"
#include <stdlib.h>
#include <string.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "vendor/dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "vendor/dr_wav.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
extern "C" {
#include "vendor/stb_vorbis.c"
}
#pragma GCC diagnostic pop

using namespace v8;

// Decoded channel cap — bounds memory for absurd channel counts. Web Audio
// allows up to 32 channels; files beyond that are truncated.
#define NX_AUDIO_DECODE_MAX_CHANNELS 32

namespace {

// An online or offline audio context: the graph plus (for online contexts)
// the platform output sink.
typedef struct {
	nx_audio_graph *graph;
	nx_audio_sink_t *sink; // NULL for offline contexts / after close()
} nx_audio_ctx_t;

void free_audio_ctx(nx_audio_ctx_t *ctx) {
	if (ctx->sink) {
		nx_audio_sink_detach(ctx->sink);
		ctx->sink = NULL;
	}
	nx_audio_graph_unref(ctx->graph);
	delete ctx;
}

nx_audio_ctx_t *get_ctx(Isolate *iso, Local<Value> v) {
	nx_audio_ctx_t *ctx = nx::Unwrap<nx_audio_ctx_t>(v);
	if (!ctx)
		nx_throw(iso, "expected AudioContext handle");
	return ctx;
}

nx_audio_node *get_node(Isolate *iso, Local<Value> v) {
	nx_audio_node *n = nx::Unwrap<nx_audio_node>(v);
	if (!n)
		nx_throw(iso, "expected AudioNode handle");
	return n;
}

double arg_f64(const FunctionCallbackInfo<Value> &info, int i) {
	double v = 0;
	if (!info[i]->NumberValue(info.GetIsolate()->GetCurrentContext()).To(&v))
		v = 0;
	return v;
}

int arg_i32(const FunctionCallbackInfo<Value> &info, int i) {
	int32_t v = 0;
	if (!info[i]->Int32Value(info.GetIsolate()->GetCurrentContext()).To(&v))
		v = 0;
	return v;
}

// ---------------------------------------------------------------------------
// Context lifecycle
// ---------------------------------------------------------------------------

// audioContextNew(sampleRate, offline) -> handle
void nx_audio_context_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double sample_rate = arg_f64(info, 0);
	bool offline = info[1]->BooleanValue(iso);
	if (!(sample_rate >= 8000 && sample_rate <= 192000)) {
		iso->ThrowException(Exception::RangeError(
		    nx_str(iso, "sampleRate must be between 8000 and 192000")));
		return;
	}
	nx_audio_graph *graph = nx_audio_graph_create(sample_rate);
	nx_audio_sink_t *sink = NULL;
	if (!offline) {
		const char *err = NULL;
		sink = nx_audio_sink_attach(graph, &err);
		if (!sink) {
			nx_audio_graph_unref(graph);
			nx_throw(iso, err ? err : "failed to open audio output");
			return;
		}
	}
	nx_audio_ctx_t *ctx = new nx_audio_ctx_t{graph, sink};
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_audio_ctx_t>(iso, obj, ctx, free_audio_ctx);
	info.GetReturnValue().Set(obj);
}

void nx_audio_context_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	if (ctx->sink) {
		nx_audio_sink_detach(ctx->sink);
		ctx->sink = NULL;
	}
	// `closed` is read by render threads under the graph mutex, so the write
	// must be guarded too (for an online context the sink is already detached
	// at this point, but an OfflineAudioContext render may be in flight).
	std::lock_guard<std::mutex> lock(ctx->graph->mutex);
	ctx->graph->closed = true;
}

void nx_audio_context_suspend(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	nx_audio_graph_set_suspended(ctx->graph, true);
}

void nx_audio_context_resume(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	nx_audio_graph_set_suspended(ctx->graph, false);
}

void nx_audio_context_current_time(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	info.GetReturnValue().Set(
	    Number::New(iso, nx_audio_graph_current_time(ctx->graph)));
}

// ---------------------------------------------------------------------------
// Nodes
// ---------------------------------------------------------------------------

void release_node(nx_audio_node *n) { nx_audio_node_release(n); }

void nx_audio_context_destination(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	// The destination node is graph-owned; the wrapper still holds a graph
	// ref (released via nx_audio_node_release, which special-cases it).
	nx_audio_graph_ref(ctx->graph);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_audio_node>(iso, obj, ctx->graph->destination, release_node);
	info.GetReturnValue().Set(obj);
}

// audioNodeNew(ctx, type) -> handle
void nx_audio_node_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	int type = arg_i32(info, 1);
	if (type < NX_AUDIO_NODE_GAIN || type > NX_AUDIO_NODE_BUFFER_SOURCE) {
		nx_throw(iso, "invalid AudioNode type");
		return;
	}
	nx_audio_node *n =
	    nx_audio_node_create(ctx->graph, (nx_audio_node_type)type);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_audio_node>(iso, obj, n, release_node);
	info.GetReturnValue().Set(obj);
}

void nx_audio_node_connect_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *src = get_node(iso, info[0]);
	nx_audio_node *dst = src ? get_node(iso, info[1]) : NULL;
	if (!src || !dst)
		return;
	if (src->graph != dst->graph) {
		nx_throw(iso, "cannot connect nodes from different AudioContexts");
		return;
	}
	nx_audio_node_connect(src, dst);
}

void nx_audio_node_disconnect_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *src = get_node(iso, info[0]);
	if (!src)
		return;
	nx_audio_node *dst = NULL;
	if (!info[1]->IsUndefined() && !info[1]->IsNull()) {
		dst = get_node(iso, info[1]);
		if (!dst)
			return;
	}
	nx_audio_node_disconnect(src, dst);
}

// ---------------------------------------------------------------------------
// Params — addressed as (node, index)
// ---------------------------------------------------------------------------

nx_audio_param *get_param(Isolate *iso,
                          const FunctionCallbackInfo<Value> &info,
                          nx_audio_node **node_out) {
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return NULL;
	nx_audio_param *p = nx_audio_node_param(n, arg_i32(info, 1));
	if (!p) {
		nx_throw(iso, "invalid AudioParam index");
		return NULL;
	}
	*node_out = n;
	return p;
}

void nx_audio_param_value_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n;
	nx_audio_param *p = get_param(iso, info, &n);
	if (!p)
		return;
	info.GetReturnValue().Set(Number::New(iso, nx_audio_param_value(n, p)));
}

void nx_audio_param_set_value_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n;
	nx_audio_param *p = get_param(iso, info, &n);
	if (!p)
		return;
	nx_audio_param_set_value(n, p, (float)arg_f64(info, 2));
}

// audioParamSchedule(node, idx, type, time, value, timeConstant)
void nx_audio_param_schedule_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n;
	nx_audio_param *p = get_param(iso, info, &n);
	if (!p)
		return;
	int type = arg_i32(info, 2);
	if (type < NX_AUDIO_PARAM_SET_VALUE || type > NX_AUDIO_PARAM_SET_TARGET) {
		nx_throw(iso, "invalid AudioParam event type");
		return;
	}
	nx_audio_param_schedule(n, p, (nx_audio_param_event_type)type,
	                        arg_f64(info, 3), (float)arg_f64(info, 4),
	                        arg_f64(info, 5));
}

// audioParamSetValueCurve(node, idx, curve: Float32Array, startTime, duration)
void nx_audio_param_set_value_curve_cb(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n;
	nx_audio_param *p = get_param(iso, info, &n);
	if (!p)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf) {
		nx_throw(iso, "expected Float32Array curve");
		return;
	}
	nx_audio_param_set_value_curve(n, p, (const float *)buf,
	                               size / sizeof(float), arg_f64(info, 3),
	                               arg_f64(info, 4));
}

void nx_audio_param_cancel_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n;
	nx_audio_param *p = get_param(iso, info, &n);
	if (!p)
		return;
	nx_audio_param_cancel(n, p, arg_f64(info, 2));
}

// ---------------------------------------------------------------------------
// AudioBufferSourceNode
// ---------------------------------------------------------------------------

// audioSourceSetBuffer(node, channels: Float32Array[], length, sampleRate)
//
// The channel data is NOT copied: the render thread reads the Float32Arrays'
// backing stores directly (kept alive via shared_ptr<BackingStore>). This is
// the zero-copy equivalent of the spec's "acquire the content" step.
void nx_audio_source_set_buffer_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return;
	if (!info[1]->IsArray()) {
		nx_throw(iso, "expected array of Float32Array channel data");
		return;
	}
	Local<Array> arr = info[1].As<Array>();
	uint32_t num_channels = arr->Length();
	if (num_channels == 0 || num_channels > NX_AUDIO_DECODE_MAX_CHANNELS) {
		nx_throw(iso, "unsupported number of channels");
		return;
	}
	std::vector<const float *> channels;
	std::vector<std::shared_ptr<void>> holds;
	uint32_t length = (uint32_t)arg_f64(info, 2);
	for (uint32_t i = 0; i < num_channels; i++) {
		Local<Value> v;
		if (!arr->Get(context, i).ToLocal(&v))
			return;
		if (!v->IsFloat32Array()) {
			nx_throw(iso, "expected Float32Array channel data");
			return;
		}
		Local<Float32Array> ta = v.As<Float32Array>();
		// Never trust the `length` argument beyond what the typed arrays
		// actually contain — the render thread reads the backing stores
		// directly, so an oversized `length` would be an OOB read.
		uint32_t elements = (uint32_t)(ta->ByteLength() / sizeof(float));
		if (elements < length)
			length = elements;
		std::shared_ptr<BackingStore> bs = ta->Buffer()->GetBackingStore();
		channels.push_back(reinterpret_cast<const float *>(
		    static_cast<uint8_t *>(bs->Data()) + ta->ByteOffset()));
		holds.push_back(std::move(bs));
	}
	double sample_rate = arg_f64(info, 3);
	nx_audio_source_set_buffer(n, channels.data(), (int)num_channels, length,
	                           sample_rate, std::move(holds));
}

// audioSourceStart(node, when, offset, duration) — duration < 0 = unlimited
void nx_audio_source_start_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return;
	nx_audio_source_start(n, arg_f64(info, 1), arg_f64(info, 2),
	                      arg_f64(info, 3));
}

void nx_audio_source_stop_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return;
	nx_audio_source_stop(n, arg_f64(info, 1));
}

void nx_audio_source_set_loop_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return;
	nx_audio_source_set_loop(n, info[1]->BooleanValue(iso), arg_f64(info, 2),
	                         arg_f64(info, 3));
}

void nx_audio_source_state_cb(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_node *n = get_node(iso, info[0]);
	if (!n)
		return;
	info.GetReturnValue().Set(
	    Integer::New(iso, nx_audio_source_playback_state(n)));
}

// ---------------------------------------------------------------------------
// decodeAudioData
// ---------------------------------------------------------------------------

enum AudioFormat {
	AUDIO_FORMAT_UNKNOWN,
	AUDIO_FORMAT_MP3,
	AUDIO_FORMAT_WAV,
	AUDIO_FORMAT_OGG,
};

AudioFormat identify_audio_format(const uint8_t *data, size_t size) {
	if (size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WAVE", 4))
		return AUDIO_FORMAT_WAV;
	if (size >= 4 && !memcmp(data, "OggS", 4))
		return AUDIO_FORMAT_OGG;
	if (size >= 3 && !memcmp(data, "ID3", 3))
		return AUDIO_FORMAT_MP3;
	// MPEG audio frame sync: 11 set bits.
	if (size >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0)
		return AUDIO_FORMAT_MP3;
	return AUDIO_FORMAT_UNKNOWN;
}

struct decode_audio_t {
	const char *err_str = nullptr;
	uint8_t *input = nullptr;
	size_t input_size = 0;
	Global<Value> buffer_val; // pins the input ArrayBuffer during decode
	float *channels[NX_AUDIO_DECODE_MAX_CHANNELS] = {};
	int num_channels = 0;
	uint32_t length = 0; // frames
	uint32_t sample_rate = 0;
};

// Split interleaved f32 frames into per-channel malloc'd planar arrays.
// Returns false on allocation failure (frees what it allocated).
bool deinterleave(decode_audio_t *data, const float *frames, uint64_t count,
                  int channels) {
	if (channels > NX_AUDIO_DECODE_MAX_CHANNELS)
		channels = NX_AUDIO_DECODE_MAX_CHANNELS;
	for (int c = 0; c < channels; c++) {
		data->channels[c] = (float *)malloc(count * sizeof(float));
		if (!data->channels[c]) {
			for (int j = 0; j < c; j++) {
				free(data->channels[j]);
				data->channels[j] = nullptr;
			}
			return false;
		}
	}
	for (uint64_t i = 0; i < count; i++)
		for (int c = 0; c < channels; c++)
			data->channels[c][i] = frames[i * data->num_channels + c];
	return true;
}

void decode_audio_work(nx_work_t *req) {
	decode_audio_t *data = (decode_audio_t *)req->data;
	AudioFormat format = identify_audio_format(data->input, data->input_size);
	if (format == AUDIO_FORMAT_MP3) {
		drmp3_config cfg;
		drmp3_uint64 frame_count;
		float *frames = drmp3_open_memory_and_read_pcm_frames_f32(
		    data->input, data->input_size, &cfg, &frame_count, NULL);
		if (!frames) {
			data->err_str = "Failed to decode MP3";
			return;
		}
		data->num_channels = (int)cfg.channels;
		data->sample_rate = cfg.sampleRate;
		data->length = (uint32_t)frame_count;
		if (!deinterleave(data, frames, frame_count, (int)cfg.channels))
			data->err_str = "Out of memory decoding MP3";
		drmp3_free(frames, NULL);
	} else if (format == AUDIO_FORMAT_WAV) {
		drwav wav;
		if (!drwav_init_memory(&wav, data->input, data->input_size, NULL)) {
			data->err_str = "Failed to decode WAV";
			return;
		}
		drwav_uint64 frame_count = wav.totalPCMFrameCount;
		float *frames =
		    (float *)malloc(frame_count * wav.channels * sizeof(float));
		if (!frames) {
			drwav_uninit(&wav);
			data->err_str = "Out of memory decoding WAV";
			return;
		}
		drwav_read_pcm_frames_f32(&wav, frame_count, frames);
		data->num_channels = (int)wav.channels;
		data->sample_rate = wav.sampleRate;
		data->length = (uint32_t)frame_count;
		if (!deinterleave(data, frames, frame_count, (int)wav.channels))
			data->err_str = "Out of memory decoding WAV";
		free(frames);
		drwav_uninit(&wav);
	} else if (format == AUDIO_FORMAT_OGG) {
		int verr = 0;
		stb_vorbis *v = stb_vorbis_open_memory(data->input,
		                                       (int)data->input_size, &verr,
		                                       NULL);
		if (!v) {
			data->err_str = "Failed to decode OGG Vorbis";
			return;
		}
		stb_vorbis_info vi = stb_vorbis_get_info(v);
		uint32_t total = stb_vorbis_stream_length_in_samples(v);
		int channels = vi.channels;
		if (channels > NX_AUDIO_DECODE_MAX_CHANNELS)
			channels = NX_AUDIO_DECODE_MAX_CHANNELS;
		data->num_channels = channels;
		data->sample_rate = vi.sample_rate;
		data->length = total;
		bool oom = false;
		for (int c = 0; c < channels; c++) {
			data->channels[c] = (float *)malloc((size_t)total * sizeof(float));
			if (!data->channels[c]) {
				oom = true;
				break;
			}
		}
		if (oom) {
			for (int c = 0; c < channels; c++) {
				free(data->channels[c]);
				data->channels[c] = nullptr;
			}
			stb_vorbis_close(v);
			data->err_str = "Out of memory decoding OGG Vorbis";
			return;
		}
		// stb_vorbis fills planar output buffers directly.
		float *bufs[NX_AUDIO_DECODE_MAX_CHANNELS];
		uint32_t offset = 0;
		while (offset < total) {
			for (int c = 0; c < channels; c++)
				bufs[c] = data->channels[c] + offset;
			int got = stb_vorbis_get_samples_float(v, channels, bufs,
			                                       (int)(total - offset));
			if (got <= 0)
				break;
			offset += (uint32_t)got;
		}
		data->length = offset;
		stb_vorbis_close(v);
	} else {
		data->err_str = "Unsupported audio format";
	}
	if (!data->err_str && (data->length == 0 || data->num_channels == 0))
		data->err_str = "Audio file contains no audio data";
}

MaybeLocal<Value> decode_audio_after(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	decode_audio_t *data = (decode_audio_t *)req->data;
	data->buffer_val.Reset();
	if (data->err_str) {
		for (int c = 0; c < NX_AUDIO_DECODE_MAX_CHANNELS; c++) {
			free(data->channels[c]);
			data->channels[c] = nullptr;
		}
		iso->ThrowException(Exception::Error(nx_str(iso, data->err_str)));
		return MaybeLocal<Value>();
	}
	int channels = data->num_channels > NX_AUDIO_DECODE_MAX_CHANNELS
	                   ? NX_AUDIO_DECODE_MAX_CHANNELS
	                   : data->num_channels;
	Local<Array> channel_data = Array::New(iso, channels);
	for (int c = 0; c < channels; c++) {
		std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
		    data->channels[c], (size_t)data->length * sizeof(float),
		    [](void *p, size_t, void *) { free(p); }, nullptr);
		data->channels[c] = nullptr; // ownership moved to the ArrayBuffer
		Local<ArrayBuffer> ab = ArrayBuffer::New(iso, std::move(bs));
		channel_data->Set(context, c, ab).Check();
	}
	Local<Object> result = Object::New(iso);
	result->Set(context, nx_str(iso, "channelData"), channel_data).Check();
	result->Set(context, nx_str(iso, "length"),
	            Number::New(iso, (double)data->length))
	    .Check();
	result->Set(context, nx_str(iso, "sampleRate"),
	            Integer::NewFromUnsigned(iso, data->sample_rate))
	    .Check();
	return result.As<Value>();
}

// audioDecode(buf) -> Promise<{ channelData: ArrayBuffer[], length, sampleRate }>
void nx_audio_decode(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[0]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	NX_INIT_WORK_T_CPP(decode_audio_t);
	data->buffer_val.Reset(iso, info[0]);
	data->input = buf;
	data->input_size = size;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, decode_audio_work, decode_audio_after));
}

// ---------------------------------------------------------------------------
// OfflineAudioContext rendering
// ---------------------------------------------------------------------------

struct offline_render_t {
	const char *err_str = nullptr;
	nx_audio_graph *graph = nullptr; // ref held for the duration of the render
	Global<Value> ctx_val;           // pins the JS context wrapper
	float *channels[NX_AUDIO_CHANNELS] = {};
	int num_channels = 0;
	uint32_t length = 0;
	~offline_render_t() {
		if (graph)
			nx_audio_graph_unref(graph);
	}
};

void offline_render_work(nx_work_t *req) {
	offline_render_t *data = (offline_render_t *)req->data;
	nx_audio_graph_render_offline(data->graph, data->channels,
	                              data->num_channels, data->length);
}

MaybeLocal<Value> offline_render_after(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	offline_render_t *data = (offline_render_t *)req->data;
	data->ctx_val.Reset();
	Local<Array> channel_data = Array::New(iso, data->num_channels);
	for (int c = 0; c < data->num_channels; c++) {
		std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
		    data->channels[c], (size_t)data->length * sizeof(float),
		    [](void *p, size_t, void *) { free(p); }, nullptr);
		data->channels[c] = nullptr;
		Local<ArrayBuffer> ab = ArrayBuffer::New(iso, std::move(bs));
		channel_data->Set(context, c, ab).Check();
	}
	return channel_data.As<Value>();
}

// audioOfflineRender(ctx, numberOfChannels, length) -> Promise<ArrayBuffer[]>
void nx_audio_offline_render(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_audio_ctx_t *ctx = get_ctx(iso, info[0]);
	if (!ctx)
		return;
	int num_channels = arg_i32(info, 1);
	uint32_t length = (uint32_t)arg_f64(info, 2);
	if (num_channels < 1 || num_channels > NX_AUDIO_CHANNELS) {
		iso->ThrowException(Exception::RangeError(
		    nx_str(iso, "numberOfChannels must be 1 or 2")));
		return;
	}
	if (length == 0) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "length must be at least 1")));
		return;
	}
	NX_INIT_WORK_T_CPP(offline_render_t);
	data->graph = ctx->graph;
	nx_audio_graph_ref(ctx->graph);
	data->ctx_val.Reset(iso, info[0]);
	data->num_channels = num_channels;
	data->length = length;
	for (int c = 0; c < num_channels; c++) {
		data->channels[c] = (float *)calloc(length, sizeof(float));
		if (!data->channels[c]) {
			for (int j = 0; j < c; j++)
				free(data->channels[j]);
			req->data_dtor(data);
			delete req;
			nx_throw_oom(iso, (size_t)length * sizeof(float));
			return;
		}
	}
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, offline_render_work, offline_render_after));
}

} // namespace

void nx_init_audio(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "audioContextNew", nx_audio_context_new);
	NX_SET_FUNC(init_obj, "audioContextClose", nx_audio_context_close);
	NX_SET_FUNC(init_obj, "audioContextSuspend", nx_audio_context_suspend);
	NX_SET_FUNC(init_obj, "audioContextResume", nx_audio_context_resume);
	NX_SET_FUNC(init_obj, "audioContextCurrentTime",
	            nx_audio_context_current_time);
	NX_SET_FUNC(init_obj, "audioContextDestination",
	            nx_audio_context_destination);
	NX_SET_FUNC(init_obj, "audioNodeNew", nx_audio_node_new);
	NX_SET_FUNC(init_obj, "audioNodeConnect", nx_audio_node_connect_cb);
	NX_SET_FUNC(init_obj, "audioNodeDisconnect", nx_audio_node_disconnect_cb);
	NX_SET_FUNC(init_obj, "audioParamValue", nx_audio_param_value_cb);
	NX_SET_FUNC(init_obj, "audioParamSetValue", nx_audio_param_set_value_cb);
	NX_SET_FUNC(init_obj, "audioParamSchedule", nx_audio_param_schedule_cb);
	NX_SET_FUNC(init_obj, "audioParamSetValueCurve",
	            nx_audio_param_set_value_curve_cb);
	NX_SET_FUNC(init_obj, "audioParamCancel", nx_audio_param_cancel_cb);
	NX_SET_FUNC(init_obj, "audioSourceSetBuffer", nx_audio_source_set_buffer_cb);
	NX_SET_FUNC(init_obj, "audioSourceStart", nx_audio_source_start_cb);
	NX_SET_FUNC(init_obj, "audioSourceStop", nx_audio_source_stop_cb);
	NX_SET_FUNC(init_obj, "audioSourceSetLoop", nx_audio_source_set_loop_cb);
	NX_SET_FUNC(init_obj, "audioSourceState", nx_audio_source_state_cb);
	NX_SET_FUNC(init_obj, "audioDecode", nx_audio_decode);
	NX_SET_FUNC(init_obj, "audioOfflineRender", nx_audio_offline_render);
}
