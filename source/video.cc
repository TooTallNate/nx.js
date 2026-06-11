// Video element native bindings.
//
// PORTABLE (compiled into both the device runtime and the host nxjs-test
// binary). The heavy lifting lives in media-decoder.cc (ffmpeg demux/decode
// on a dedicated thread); this module is the V8 glue plus the drawImage
// integration.
//
// drawImage integration: nx_video_t embeds an nx_image_t as its FIRST member,
// making a Video JS object directly drawable by canvas.cc (which unwraps any
// wrapped object as nx_image_t — width/height/premul-BGRA `data` + the
// SkImage cache slot). Each presented frame pointer-swaps into `image.data`
// and drops the SkImage memo, exactly like the IR sensor's per-frame path.
#include "video.h"
#include "async.h"
#include "audio-graph.h"
#include "audio.h"
#include "error.h"
#include "image.h"
#include "media-decoder.h"
#include "util.h"
#include "wrap.h"
#include <stdlib.h>
#include <string.h>

using namespace v8;

namespace {

struct nx_video_t {
	// MUST be first: canvas.cc draws any wrapped object as an nx_image_t.
	nx_image_t image;
	nx_media_t *media = nullptr;
	// Stream-source node owned by this video (released on close, AFTER the
	// decode thread is joined). NULL when the media has no audio track or no
	// audio context was attached.
	nx_audio_node *audio_node = nullptr;
	bool closed = false;
};

// Tear down the current media (joins the decode thread) so the handle can be
// reloaded with a new source. Safe to call with nothing loaded.
void reset_media(nx_video_t *v) {
	if (v->media) {
		nx_media_destroy(v->media); // joins the decode thread
		v->media = nullptr;
	}
	if (v->audio_node) {
		// Released strictly after the decode thread is gone.
		nx_audio_node_release(v->audio_node);
		v->audio_node = nullptr;
	}
	nx_image_release_cache(&v->image);
	free(v->image.data);
	v->image.data = nullptr;
	v->image.width = v->image.height = 0;
}

void close_video(nx_video_t *v) {
	if (v->closed)
		return;
	v->closed = true;
	reset_media(v);
}

void free_video(nx_video_t *v) {
	close_video(v);
	delete v;
}

nx_video_t *get_video(Isolate *iso, Local<Value> val) {
	nx_video_t *v = nx::Unwrap<nx_video_t>(val);
	if (!v)
		nx_throw(iso, "expected Video handle");
	return v;
}

double arg_f64(const FunctionCallbackInfo<Value> &info, int i) {
	double v = 0;
	if (!info[i]->NumberValue(info.GetIsolate()->GetCurrentContext()).To(&v))
		v = 0;
	return v;
}

void nx_video_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> obj = nx::NewWrapped(iso);
	nx_video_t *v = new nx_video_t();
	memset(&v->image, 0, sizeof(v->image));
	v->image.magic = NX_IMAGE_MAGIC;
	v->image.format = FORMAT_UNKNOWN;
	nx::Wrap<nx_video_t>(iso, obj, v, free_video);
	info.GetReturnValue().Set(obj);
}

// ---------------------------------------------------------------------------
// videoLoad(video, path | null, buffer | null) -> Promise<metadata>
// ---------------------------------------------------------------------------

struct video_load_t {
	nx_video_t *video = nullptr;
	Global<Value> video_val; // pins the wrapper during the load
	char *path = nullptr;    // owned copy (file-backed load)
	const uint8_t *mem = nullptr;
	size_t mem_size = 0;
	// Strong ref to the memory buffer for the duration of the open (and,
	// via nx_media_open, handed to the media for its whole lifetime) — a
	// concurrent reset/close on the main thread can never unpin it.
	std::shared_ptr<BackingStore> mem_store;
	nx_media_t *media = nullptr;
	char err_buf[256] = {};
	~video_load_t() { free(path); }
};

void video_load_work(nx_work_t *req) {
	video_load_t *data = (video_load_t *)req->data;
	data->media =
	    nx_media_open(data->path, data->mem, data->mem_size, data->mem_store,
	                  data->err_buf, sizeof(data->err_buf));
}

MaybeLocal<Value> video_load_after(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	video_load_t *data = (video_load_t *)req->data;
	nx_video_t *v = data->video;
	data->video_val.Reset();
	if (!data->media) {
		iso->ThrowException(Exception::Error(nx_str_lossy(iso, data->err_buf)));
		return MaybeLocal<Value>();
	}
	if (v->closed) {
		// The video was closed while the open was in flight.
		nx_media_destroy(data->media);
		iso->ThrowException(Exception::Error(nx_str(iso, "Video was closed")));
		return MaybeLocal<Value>();
	}
	// Racing loads (src changed while this open was in flight): last one
	// wins — tear down whatever an earlier load installed.
	reset_media(v);
	v->media = data->media;
	int width = nx_media_width(data->media);
	int height = nx_media_height(data->media);
	if (nx_media_has_video(data->media)) {
		uint8_t *buf = (uint8_t *)nx_alloc(iso, (size_t)width * height * 4);
		if (!buf) {
			nx_media_destroy(v->media);
			v->media = nullptr;
			return MaybeLocal<Value>();
		}
		memset(buf, 0, (size_t)width * height * 4);
		// Opaque black until the first frame presents.
		for (size_t i = 3; i < (size_t)width * height * 4; i += 4)
			buf[i] = 0xff;
		v->image.width = (uint32_t)width;
		v->image.height = (uint32_t)height;
		v->image.data = buf;
	}
	Local<Object> result = Object::New(iso);
	result->Set(context, nx_str(iso, "width"), Integer::New(iso, width))
	    .Check();
	result->Set(context, nx_str(iso, "height"), Integer::New(iso, height))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "duration"),
	          Number::New(iso, nx_media_duration(data->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "hasAudio"),
	          Boolean::New(iso, nx_media_has_audio(data->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "hasVideo"),
	          Boolean::New(iso, nx_media_has_video(data->media)))
	    .Check();
	return result.As<Value>();
}

void nx_video_load(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (!v)
		return;
	if (v->closed) {
		nx_throw(iso, "Video was closed");
		return;
	}
	reset_media(v); // support changing `src`
	NX_INIT_WORK_T_CPP(video_load_t);
	data->video = v;
	data->video_val.Reset(iso, info[0]);
	if (info[1]->IsString()) {
		String::Utf8Value path(iso, info[1]);
		if (*path)
			data->path = strdup(*path);
	} else if (info[2]->IsArrayBuffer()) {
		Local<ArrayBuffer> ab = info[2].As<ArrayBuffer>();
		std::shared_ptr<BackingStore> bs = ab->GetBackingStore();
		data->mem = (const uint8_t *)bs->Data();
		data->mem_size = bs->ByteLength();
		data->mem_store = std::move(bs);
	}
	if (!data->path && !data->mem) {
		req->data_dtor(data);
		delete req;
		nx_throw(iso, "expected a path string or ArrayBuffer");
		return;
	}
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, video_load_work, video_load_after));
}

// ---------------------------------------------------------------------------
// Transport + presentation
// ---------------------------------------------------------------------------

void nx_video_play(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (v && v->media)
		nx_media_play(v->media);
}

void nx_video_pause(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (v && v->media)
		nx_media_pause(v->media);
}

void nx_video_seek(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (v && v->media)
		nx_media_seek(v->media, arg_f64(info, 1));
}

void nx_video_set_loop(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (v && v->media)
		nx_media_set_loop(v->media, info[1]->BooleanValue(iso));
}

// videoTick(video) -> boolean (a new frame was presented)
void nx_video_tick(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (!v || !v->media || !v->image.data)
		return;
	if (nx_media_present(v->media, &v->image.data)) {
		nx_image_release_cache(&v->image);
		info.GetReturnValue().Set(true);
	} else {
		info.GetReturnValue().Set(false);
	}
}

// videoState(video) -> { currentTime, ended, seeking, buffered, error? }
void nx_video_state(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_video_t *v = get_video(iso, info[0]);
	if (!v || !v->media)
		return;
	Local<Object> result = Object::New(iso);
	result
	    ->Set(context, nx_str(iso, "currentTime"),
	          Number::New(iso, nx_media_current_time(v->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "ended"),
	          Boolean::New(iso, nx_media_ended(v->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "seeking"),
	          Boolean::New(iso, nx_media_seeking(v->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "buffered"),
	          Integer::NewFromUnsigned(iso,
	                                   nx_media_buffered_frames(v->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "presentedFrames"),
	          Number::New(iso, (double)nx_media_presented_frames(v->media)))
	    .Check();
	result
	    ->Set(context, nx_str(iso, "droppedFrames"),
	          Number::New(iso, (double)nx_media_dropped_frames(v->media)))
	    .Check();
	const char *err = nx_media_error(v->media);
	if (err) {
		result->Set(context, nx_str(iso, "error"), nx_str_lossy(iso, err))
		    .Check();
	}
	info.GetReturnValue().Set(result);
}

// videoCreateAudioNode(video, audioCtxHandle) -> stream node handle
//
// The returned wrapper has NO finalizer: the node's lifetime is owned by the
// video (released in close_video, strictly after the decode thread joins, so
// the producer can never touch a freed node). The JS Video element keeps both
// objects alive together.
void nx_video_create_audio_node(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (!v)
		return;
	nx_audio_ctx_t *ctx = nx::Unwrap<nx_audio_ctx_t>(info[1]);
	if (!ctx) {
		nx_throw(iso, "expected AudioContext handle");
		return;
	}
	if (!v->media || !nx_media_has_audio(v->media) || v->audio_node) {
		info.GetReturnValue().SetNull();
		return;
	}
	nx_audio_node *node =
	    nx_audio_node_create(ctx->graph, NX_AUDIO_NODE_STREAM_SOURCE);
	v->audio_node = node;
	nx_media_set_audio_node(v->media, node, ctx->graph->sample_rate);
	Local<Object> obj = nx::NewWrapped(iso);
	obj->SetAlignedPointerInInternalField(0, node,
	                                      kEmbedderDataTypeTagDefault);
	info.GetReturnValue().Set(obj);
}

void nx_video_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_video_t *v = get_video(iso, info[0]);
	if (v)
		close_video(v);
}

} // namespace

void nx_init_video(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "videoNew", nx_video_new);
	NX_SET_FUNC(init_obj, "videoLoad", nx_video_load);
	NX_SET_FUNC(init_obj, "videoPlay", nx_video_play);
	NX_SET_FUNC(init_obj, "videoPause", nx_video_pause);
	NX_SET_FUNC(init_obj, "videoSeek", nx_video_seek);
	NX_SET_FUNC(init_obj, "videoSetLoop", nx_video_set_loop);
	NX_SET_FUNC(init_obj, "videoTick", nx_video_tick);
	NX_SET_FUNC(init_obj, "videoState", nx_video_state);
	NX_SET_FUNC(init_obj, "videoCreateAudioNode", nx_video_create_audio_node);
	NX_SET_FUNC(init_obj, "videoClose", nx_video_close);
}
