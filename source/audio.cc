#include "async.h"
#include "error.h"
#include "types.h"
#include "util.h"
#include <malloc.h>
#include <string.h>
#include <switch.h>

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

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_NUM_VOICES 24
#define AUDIO_ALIGN 0x1000
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

namespace {

bool audio_initialized = false;
AudioDriver audio_driver;
bool voice_in_use[AUDIO_NUM_VOICES];
void *audio_mempool_ptr = NULL;
size_t audio_mempool_size = 0;
int audio_mempool_id = -1;
AudioDriverWaveBuf voice_wavebufs[AUDIO_NUM_VOICES];

const AudioRendererConfig arConfig = {
    .output_rate = AudioRendererOutputRate_48kHz,
    .num_voices = AUDIO_NUM_VOICES,
    .num_effects = 0,
    .num_sinks = 1,
    .num_mix_objs = 1,
    .num_mix_buffers = 2,
};

// ---- async decode ----
typedef struct {
	const char *err_str;
	uint8_t *input;
	size_t input_size;
	char *mime_type; // owned copy
	int16_t *pcm_data;
	uint32_t sample_rate;
	uint32_t channels;
	uint64_t total_samples;
	Global<Value> buffer_val;
} decode_audio_t;

void decode_audio_work(nx_work_t *req) {
	decode_audio_t *data = (decode_audio_t *)req->data;
	if (!strcmp(data->mime_type, "audio/mpeg") ||
	    !strcmp(data->mime_type, "audio/mp3")) {
		drmp3_config cfg;
		drmp3_uint64 frame_count;
		drmp3_int16 *frames = drmp3_open_memory_and_read_pcm_frames_s16(
		    data->input, data->input_size, &cfg, &frame_count, NULL);
		if (!frames) {
			data->err_str = "Failed to decode MP3";
			return;
		}
		data->pcm_data = frames;
		data->sample_rate = cfg.channels > 0 ? cfg.sampleRate : 44100;
		data->channels = cfg.channels;
		data->total_samples = frame_count;
	} else if (!strcmp(data->mime_type, "audio/wav") ||
	           !strcmp(data->mime_type, "audio/wave") ||
	           !strcmp(data->mime_type, "audio/x-wav")) {
		drwav wav;
		if (!drwav_init_memory(&wav, data->input, data->input_size, NULL)) {
			data->err_str = "Failed to decode WAV";
			return;
		}
		drwav_uint64 frame_count = wav.totalPCMFrameCount;
		int16_t *frames =
		    (int16_t *)malloc(frame_count * wav.channels * sizeof(int16_t));
		if (!frames) {
			drwav_uninit(&wav);
			data->err_str = "Out of memory decoding WAV";
			return;
		}
		drwav_read_pcm_frames_s16(&wav, frame_count, frames);
		data->pcm_data = frames;
		data->sample_rate = wav.sampleRate;
		data->channels = wav.channels;
		data->total_samples = frame_count;
		drwav_uninit(&wav);
	} else if (!strcmp(data->mime_type, "audio/ogg") ||
	           !strcmp(data->mime_type, "audio/vorbis")) {
		int channels, sample_rate;
		short *output;
		int samples = stb_vorbis_decode_memory(data->input,
		                                       (int)data->input_size, &channels,
		                                       &sample_rate, &output);
		if (samples < 0) {
			data->err_str = "Failed to decode OGG Vorbis";
			return;
		}
		data->pcm_data = output;
		data->sample_rate = sample_rate;
		data->channels = channels;
		data->total_samples = samples;
	} else {
		data->err_str = "Unsupported audio MIME type";
	}
}

MaybeLocal<Value> decode_audio_after(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	decode_audio_t *data = (decode_audio_t *)req->data;
	data->buffer_val.Reset();
	free(data->mime_type);
	data->mime_type = nullptr;

	if (data->err_str) {
		iso->ThrowException(Exception::Error(nx_str(iso, data->err_str)));
		if (data->pcm_data)
			free(data->pcm_data);
		data->pcm_data = nullptr;
		return MaybeLocal<Value>();
	}

	size_t pcm_byte_size =
	    data->total_samples * data->channels * sizeof(int16_t);
	size_t aligned_size = ALIGN_UP(pcm_byte_size, AUDIO_ALIGN);
	void *aligned_buf = memalign(AUDIO_ALIGN, aligned_size);
	if (!aligned_buf) {
		if (data->pcm_data)
			free(data->pcm_data);
		data->pcm_data = nullptr;
		nx_throw(iso, "Failed to allocate aligned PCM buffer");
		return MaybeLocal<Value>();
	}
	memcpy(aligned_buf, data->pcm_data, pcm_byte_size);
	memset((uint8_t *)aligned_buf + pcm_byte_size, 0,
	       aligned_size - pcm_byte_size);
	armDCacheFlush(aligned_buf, aligned_size);
	if (data->pcm_data)
		free(data->pcm_data);
	data->pcm_data = nullptr;

	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    aligned_buf, aligned_size,
	    [](void *p, size_t, void *) { free(p); }, nullptr);
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, std::move(bs));

	Local<Object> result = Object::New(iso);
	result->Set(context, nx_str(iso, "pcmData"), ab).Check();
	result->Set(context, nx_str(iso, "sampleRate"),
	            Integer::NewFromUnsigned(iso, data->sample_rate))
	    .Check();
	result->Set(context, nx_str(iso, "channels"),
	            Integer::NewFromUnsigned(iso, data->channels))
	    .Check();
	result->Set(context, nx_str(iso, "samples"),
	            Number::New(iso, (double)data->total_samples))
	    .Check();
	result->Set(context, nx_str(iso, "byteLength"),
	            Number::New(iso, (double)pcm_byte_size))
	    .Check();
	return result.As<Value>();
}

void nx_audio_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (audio_initialized)
		return;
	Result rc = audrenInitialize(&arConfig);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "audrenInitialize");
		return;
	}
	rc = audrvCreate(&audio_driver, &arConfig, 2);
	if (R_FAILED(rc)) {
		audrenExit();
		nx_throw_libnx_error(iso, rc, "audrvCreate");
		return;
	}
	audio_mempool_size = ALIGN_UP(1024 * 1024, AUDIO_ALIGN);
	audio_mempool_ptr = memalign(AUDIO_ALIGN, audio_mempool_size);
	if (!audio_mempool_ptr) {
		audrvClose(&audio_driver);
		audrenExit();
		nx_throw(iso, "Failed to allocate audio mempool");
		return;
	}
	memset(audio_mempool_ptr, 0, audio_mempool_size);
	armDCacheFlush(audio_mempool_ptr, audio_mempool_size);
	audio_mempool_id =
	    audrvMemPoolAdd(&audio_driver, audio_mempool_ptr, audio_mempool_size);
	audrvMemPoolAttach(&audio_driver, audio_mempool_id);
	static const u8 sink_channels[] = {0, 1};
	audrvDeviceSinkAdd(&audio_driver, AUDREN_DEFAULT_DEVICE_NAME, 2,
	                   sink_channels);
	rc = audrenStartAudioRenderer();
	if (R_FAILED(rc)) {
		audrvClose(&audio_driver);
		audrenExit();
		nx_throw_libnx_error(iso, rc, "audrenStartAudioRenderer");
		return;
	}
	audrvUpdate(&audio_driver);
	memset(voice_in_use, 0, sizeof(voice_in_use));
	memset(voice_wavebufs, 0, sizeof(voice_wavebufs));
	audio_initialized = true;
}

void nx_audio_exit(const FunctionCallbackInfo<Value> &info) {
	if (!audio_initialized)
		return;
	for (int i = 0; i < AUDIO_NUM_VOICES; i++)
		if (voice_in_use[i])
			audrvVoiceStop(&audio_driver, i);
	audrvUpdate(&audio_driver);
	if (audio_mempool_id >= 0) {
		audrvMemPoolDetach(&audio_driver, audio_mempool_id);
		audrvMemPoolRemove(&audio_driver, audio_mempool_id);
	}
	audrvClose(&audio_driver);
	audrenExit();
	if (audio_mempool_ptr) {
		free(audio_mempool_ptr);
		audio_mempool_ptr = NULL;
	}
	audio_initialized = false;
}

void nx_audio_decode(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[0]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	String::Utf8Value mime(iso, info[1]);
	if (!*mime)
		return;
	nx_work_t *req = new nx_work_t();
	decode_audio_t *data = new decode_audio_t();
	req->data = data;
	data->buffer_val.Reset(iso, info[0]);
	data->input = buf;
	data->input_size = size;
	data->mime_type = strdup(*mime);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, decode_audio_work, decode_audio_after));
}

// Argument helpers.
int arg_i32(const FunctionCallbackInfo<Value> &info, int i) {
	int32_t v = 0;
	(void)info[i]->Int32Value(info.GetIsolate()->GetCurrentContext()).To(&v);
	return v;
}
double arg_f64(const FunctionCallbackInfo<Value> &info, int i) {
	double v = 0;
	(void)info[i]->NumberValue(info.GetIsolate()->GetCurrentContext()).To(&v);
	return v;
}

void nx_audio_play(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!audio_initialized) {
		nx_throw(iso, "Audio not initialized");
		return;
	}
	size_t pcm_size = 0;
	uint8_t *pcm_data = NX_GetBufferSource(iso, &pcm_size, info[0]);
	if (!pcm_data)
		return;
	int voice_id = arg_i32(info, 1);
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Invalid voice ID")));
		return;
	}
	double volume = arg_f64(info, 2);
	bool loop = info[3]->BooleanValue(iso);
	int32_t sample_rate = arg_i32(info, 4);
	int32_t channels = arg_i32(info, 5);
	uint64_t total_samples = (uint64_t)arg_f64(info, 6);

	size_t aligned_size = ALIGN_UP(pcm_size, AUDIO_ALIGN);
	int pool_id = audrvMemPoolAdd(&audio_driver, pcm_data, aligned_size);
	if (pool_id < 0) {
		nx_throw(iso, "Failed to add audio mempool");
		return;
	}
	audrvMemPoolAttach(&audio_driver, pool_id);
	audrvVoiceInit(&audio_driver, voice_id, channels, PcmFormat_Int16,
	               sample_rate);
	audrvVoiceSetDestinationMix(&audio_driver, voice_id, AUDREN_FINAL_MIX_ID);
	if (channels == 1) {
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 1);
	} else {
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 0.0f, 0, 1);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 0.0f, 1, 0);
		audrvVoiceSetMixFactor(&audio_driver, voice_id, 1.0f, 1, 1);
	}
	audrvVoiceSetVolume(&audio_driver, voice_id, (float)volume);
	AudioDriverWaveBuf *wavebuf = &voice_wavebufs[voice_id];
	memset(wavebuf, 0, sizeof(AudioDriverWaveBuf));
	wavebuf->data_raw = pcm_data;
	wavebuf->size = pcm_size;
	wavebuf->start_sample_offset = 0;
	wavebuf->end_sample_offset = total_samples;
	wavebuf->is_looping = loop;
	audrvVoiceAddWaveBuf(&audio_driver, voice_id, wavebuf);
	audrvVoiceStart(&audio_driver, voice_id);
	audrvUpdate(&audio_driver);
}

void nx_audio_stop(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!audio_initialized)
		return;
	int voice_id = arg_i32(info, 0);
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Invalid voice ID")));
		return;
	}
	audrvVoiceStop(&audio_driver, voice_id);
	audrvUpdate(&audio_driver);
	voice_in_use[voice_id] = false;
}

void nx_audio_pause(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!audio_initialized)
		return;
	int voice_id = arg_i32(info, 0);
	bool paused = info[1]->BooleanValue(iso);
	audrvVoiceSetPaused(&audio_driver, voice_id, paused);
	audrvUpdate(&audio_driver);
}

void nx_audio_set_volume(const FunctionCallbackInfo<Value> &info) {
	if (!audio_initialized)
		return;
	audrvVoiceSetVolume(&audio_driver, arg_i32(info, 0),
	                    (float)arg_f64(info, 1));
	audrvUpdate(&audio_driver);
}
void nx_audio_set_pitch(const FunctionCallbackInfo<Value> &info) {
	if (!audio_initialized)
		return;
	audrvVoiceSetPitch(&audio_driver, arg_i32(info, 0),
	                   (float)arg_f64(info, 1));
	audrvUpdate(&audio_driver);
}
void nx_audio_update(const FunctionCallbackInfo<Value> &info) {
	if (audio_initialized)
		audrvUpdate(&audio_driver);
}
void nx_audio_get_played_samples(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!audio_initialized) {
		info.GetReturnValue().Set(Number::New(iso, 0));
		return;
	}
	u64 count =
	    audrvVoiceGetPlayedSampleCount(&audio_driver, arg_i32(info, 0));
	info.GetReturnValue().Set(Number::New(iso, (double)count));
}
void nx_audio_alloc_voice(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
		if (!voice_in_use[i]) {
			voice_in_use[i] = true;
			info.GetReturnValue().Set(Integer::New(iso, i));
			return;
		}
	}
	nx_throw(iso, "No free audio voices");
}
void nx_audio_free_voice(const FunctionCallbackInfo<Value> &info) {
	int voice_id = arg_i32(info, 0);
	if (voice_id >= 0 && voice_id < AUDIO_NUM_VOICES) {
		if (audio_initialized) {
			audrvVoiceStop(&audio_driver, voice_id);
			audrvUpdate(&audio_driver);
		}
		voice_in_use[voice_id] = false;
	}
}
void nx_audio_is_playing(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!audio_initialized) {
		info.GetReturnValue().Set(false);
		return;
	}
	AudioDriverWaveBuf *wb = &voice_wavebufs[arg_i32(info, 0)];
	info.GetReturnValue().Set(
	    Boolean::New(iso, wb->state == AudioDriverWaveBufState_Playing));
}

} // namespace

void nx_init_audio(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "audioInit", nx_audio_init);
	NX_SET_FUNC(init_obj, "audioExit", nx_audio_exit);
	NX_SET_FUNC(init_obj, "audioDecode", nx_audio_decode);
	NX_SET_FUNC(init_obj, "audioPlay", nx_audio_play);
	NX_SET_FUNC(init_obj, "audioStop", nx_audio_stop);
	NX_SET_FUNC(init_obj, "audioPause", nx_audio_pause);
	NX_SET_FUNC(init_obj, "audioSetVolume", nx_audio_set_volume);
	NX_SET_FUNC(init_obj, "audioSetPitch", nx_audio_set_pitch);
	NX_SET_FUNC(init_obj, "audioUpdate", nx_audio_update);
	NX_SET_FUNC(init_obj, "audioGetPlayedSamples", nx_audio_get_played_samples);
	NX_SET_FUNC(init_obj, "audioAllocVoice", nx_audio_alloc_voice);
	NX_SET_FUNC(init_obj, "audioFreeVoice", nx_audio_free_voice);
	NX_SET_FUNC(init_obj, "audioIsPlaying", nx_audio_is_playing);
}
