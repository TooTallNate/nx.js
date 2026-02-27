#include "audio.h"
#include "async.h"
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
#include "vendor/stb_vorbis.c"
#pragma GCC diagnostic pop

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_NUM_VOICES 24
#define AUDIO_ALIGN 0x1000

static bool audio_initialized = false;
static AudioDriver audio_driver;
static bool voice_in_use[AUDIO_NUM_VOICES];
static void *audio_mempool_ptr = NULL;
static size_t audio_mempool_size = 0;
static int audio_mempool_id = -1;

/* Per-voice wave buffer tracking */
static AudioDriverWaveBuf voice_wavebufs[AUDIO_NUM_VOICES];

static const AudioRendererConfig arConfig = {
	.output_rate     = AudioRendererOutputRate_48kHz,
	.num_voices      = AUDIO_NUM_VOICES,
	.num_effects     = 0,
	.num_sinks       = 1,
	.num_mix_objs    = 1,
	.num_mix_buffers = 2,
};

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

/* ── Decode async data ── */

typedef struct {
	char *err_str;
	uint8_t *input;
	size_t input_size;
	const char *mime_type;
	/* Output */
	int16_t *pcm_data;
	uint32_t sample_rate;
	uint32_t channels;
	uint64_t total_samples;
	JSValue buffer_val;
} nx_decode_audio_async_t;

/* ── Decode work (runs on thread pool) ── */

static void decode_audio_work(nx_work_t *req) {
	nx_decode_audio_async_t *data = (nx_decode_audio_async_t *)req->data;

	if (strcmp(data->mime_type, "audio/mpeg") == 0 ||
		strcmp(data->mime_type, "audio/mp3") == 0) {
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
	} else if (strcmp(data->mime_type, "audio/wav") == 0 ||
			   strcmp(data->mime_type, "audio/wave") == 0 ||
			   strcmp(data->mime_type, "audio/x-wav") == 0) {
		drwav wav;
		if (!drwav_init_memory(&wav, data->input, data->input_size, NULL)) {
			data->err_str = "Failed to decode WAV";
			return;
		}
		drwav_uint64 frame_count = wav.totalPCMFrameCount;
		int16_t *frames = malloc(frame_count * wav.channels * sizeof(int16_t));
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
	} else if (strcmp(data->mime_type, "audio/ogg") == 0 ||
			   strcmp(data->mime_type, "audio/vorbis") == 0) {
		int channels, sample_rate;
		short *output;
		int samples = stb_vorbis_decode_memory(
			data->input, (int)data->input_size, &channels, &sample_rate,
			&output);
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

/* ── Decode after-work (runs on JS thread) ── */

static JSValue decode_audio_after_work(JSContext *ctx, nx_work_t *req) {
	nx_decode_audio_async_t *data = (nx_decode_audio_async_t *)req->data;

	JS_FreeValue(ctx, data->buffer_val);
	JS_FreeCString(ctx, data->mime_type);

	if (data->err_str) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, data->err_str),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	/* Create result object with ArrayBuffer for PCM data */
	size_t pcm_byte_size =
		data->total_samples * data->channels * sizeof(int16_t);

	/* Allocate page-aligned memory for audren mempool compatibility */
	size_t aligned_size = ALIGN_UP(pcm_byte_size, AUDIO_ALIGN);
	void *aligned_buf = memalign(AUDIO_ALIGN, aligned_size);
	if (!aligned_buf) {
		if (data->pcm_data) free(data->pcm_data);
		return JS_ThrowInternalError(ctx, "Failed to allocate aligned PCM buffer");
	}
	memcpy(aligned_buf, data->pcm_data, pcm_byte_size);
	memset((uint8_t *)aligned_buf + pcm_byte_size, 0,
		   aligned_size - pcm_byte_size);
	armDCacheFlush(aligned_buf, aligned_size);

	if (data->pcm_data) free(data->pcm_data);

	JSValue ab = JS_NewArrayBuffer(
		ctx, (uint8_t *)aligned_buf, aligned_size,
		(JSFreeArrayBufferDataFunc *)free, NULL, false);

	JSValue result = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, result, "pcmData", ab);
	JS_SetPropertyStr(ctx, result, "sampleRate",
					  JS_NewUint32(ctx, data->sample_rate));
	JS_SetPropertyStr(ctx, result, "channels",
					  JS_NewUint32(ctx, data->channels));
	JS_SetPropertyStr(ctx, result, "samples",
					  JS_NewFloat64(ctx, (double)data->total_samples));
	JS_SetPropertyStr(ctx, result, "byteLength",
					  JS_NewFloat64(ctx, (double)pcm_byte_size));

	return result;
}

/* ── Native JS functions ── */

static JSValue nx_audio_init(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (audio_initialized)
		return JS_UNDEFINED;

	Result rc = audrenInitialize(&arConfig);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "audrenInitialize failed: 0x%X", rc);
		return JS_EXCEPTION;
	}

	rc = audrvCreate(&audio_driver, &arConfig, 2);
	if (R_FAILED(rc)) {
		audrenExit();
		JS_ThrowInternalError(ctx, "audrvCreate failed: 0x%X", rc);
		return JS_EXCEPTION;
	}

	/* Set up a default memory pool (1 MB, can grow later) */
	audio_mempool_size = ALIGN_UP(1024 * 1024, AUDIO_ALIGN);
	audio_mempool_ptr = memalign(AUDIO_ALIGN, audio_mempool_size);
	if (!audio_mempool_ptr) {
		audrvClose(&audio_driver);
		audrenExit();
		return JS_ThrowInternalError(ctx, "Failed to allocate audio mempool");
	}
	memset(audio_mempool_ptr, 0, audio_mempool_size);
	armDCacheFlush(audio_mempool_ptr, audio_mempool_size);

	audio_mempool_id =
		audrvMemPoolAdd(&audio_driver, audio_mempool_ptr, audio_mempool_size);
	audrvMemPoolAttach(&audio_driver, audio_mempool_id);

	/* Add device sink */
	static const u8 sink_channels[] = {0, 1};
	audrvDeviceSinkAdd(&audio_driver, AUDREN_DEFAULT_DEVICE_NAME, 2,
					   sink_channels);

	rc = audrenStartAudioRenderer();
	if (R_FAILED(rc)) {
		audrvClose(&audio_driver);
		audrenExit();
		JS_ThrowInternalError(ctx, "audrenStartAudioRenderer failed: 0x%X", rc);
		return JS_EXCEPTION;
	}

	audrvUpdate(&audio_driver);

	memset(voice_in_use, 0, sizeof(voice_in_use));
	memset(voice_wavebufs, 0, sizeof(voice_wavebufs));
	audio_initialized = true;
	return JS_UNDEFINED;
}

static JSValue nx_audio_exit(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;

	for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
		if (voice_in_use[i]) {
			audrvVoiceStop(&audio_driver, i);
		}
	}
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
	return JS_UNDEFINED;
}

static JSValue nx_audio_decode(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decode_audio_async_t);
	data->buffer_val = JS_DupValue(ctx, argv[0]);
	data->input = JS_GetArrayBuffer(ctx, &data->input_size, data->buffer_val);
	data->mime_type = JS_ToCString(ctx, argv[1]);
	if (!data->mime_type) {
		free(data);
		free(req);
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, decode_audio_work,
						  decode_audio_after_work);
}

static JSValue nx_audio_play(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (!audio_initialized) {
		return JS_ThrowInternalError(ctx, "Audio not initialized");
	}

	size_t pcm_size;
	uint8_t *pcm_data = JS_GetArrayBuffer(ctx, &pcm_size, argv[0]);
	if (!pcm_data)
		return JS_EXCEPTION;

	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[1]);
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES) {
		return JS_ThrowRangeError(ctx, "Invalid voice ID");
	}

	double volume;
	JS_ToFloat64(ctx, &volume, argv[2]);

	int loop = JS_ToBool(ctx, argv[3]);
	if (loop == -1)
		return JS_EXCEPTION;

	int32_t sample_rate;
	JS_ToInt32(ctx, &sample_rate, argv[4]);

	int32_t channels;
	JS_ToInt32(ctx, &channels, argv[5]);

	uint64_t total_samples;
	double samples_dbl;
	JS_ToFloat64(ctx, &samples_dbl, argv[6]);
	total_samples = (uint64_t)samples_dbl;

	/* Add the PCM buffer as a mempool */
	size_t aligned_size = ALIGN_UP(pcm_size, AUDIO_ALIGN);
	int pool_id = audrvMemPoolAdd(&audio_driver, pcm_data, aligned_size);
	if (pool_id < 0) {
		return JS_ThrowInternalError(ctx, "Failed to add audio mempool");
	}
	audrvMemPoolAttach(&audio_driver, pool_id);

	/* Initialize voice */
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

	/* Set up wave buffer */
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

	return JS_UNDEFINED;
}

static JSValue nx_audio_stop(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	if (voice_id < 0 || voice_id >= AUDIO_NUM_VOICES)
		return JS_ThrowRangeError(ctx, "Invalid voice ID");

	audrvVoiceStop(&audio_driver, voice_id);
	audrvUpdate(&audio_driver);
	voice_in_use[voice_id] = false;
	return JS_UNDEFINED;
}

static JSValue nx_audio_pause(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	int paused = JS_ToBool(ctx, argv[1]);
	if (paused == -1)
		return JS_EXCEPTION;
	audrvVoiceSetPaused(&audio_driver, voice_id, paused);
	audrvUpdate(&audio_driver);
	return JS_UNDEFINED;
}

static JSValue nx_audio_set_volume(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	double volume;
	JS_ToFloat64(ctx, &volume, argv[1]);
	audrvVoiceSetVolume(&audio_driver, voice_id, (float)volume);
	audrvUpdate(&audio_driver);
	return JS_UNDEFINED;
}

static JSValue nx_audio_set_pitch(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	double pitch;
	JS_ToFloat64(ctx, &pitch, argv[1]);
	audrvVoiceSetPitch(&audio_driver, voice_id, (float)pitch);
	audrvUpdate(&audio_driver);
	return JS_UNDEFINED;
}

static JSValue nx_audio_update(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	if (!audio_initialized)
		return JS_UNDEFINED;
	audrvUpdate(&audio_driver);
	return JS_UNDEFINED;
}

static JSValue nx_audio_get_played_samples(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	if (!audio_initialized)
		return JS_NewFloat64(ctx, 0);
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	u64 count = audrvVoiceGetPlayedSampleCount(&audio_driver, voice_id);
	return JS_NewFloat64(ctx, (double)count);
}

static JSValue nx_audio_alloc_voice(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	for (int i = 0; i < AUDIO_NUM_VOICES; i++) {
		if (!voice_in_use[i]) {
			voice_in_use[i] = true;
			return JS_NewInt32(ctx, i);
		}
	}
	return JS_ThrowInternalError(ctx, "No free audio voices");
}

static JSValue nx_audio_free_voice(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	if (voice_id >= 0 && voice_id < AUDIO_NUM_VOICES) {
		if (audio_initialized) {
			audrvVoiceStop(&audio_driver, voice_id);
			audrvUpdate(&audio_driver);
		}
		voice_in_use[voice_id] = false;
	}
	return JS_UNDEFINED;
}

static JSValue nx_audio_is_playing(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	if (!audio_initialized)
		return JS_FALSE;
	int voice_id;
	JS_ToInt32(ctx, &voice_id, argv[0]);
	AudioDriverWaveBuf *wb = &voice_wavebufs[voice_id];
	return JS_NewBool(ctx, wb->state == AudioDriverWaveBufState_Playing);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("audioInit", 0, nx_audio_init),
	JS_CFUNC_DEF("audioExit", 0, nx_audio_exit),
	JS_CFUNC_DEF("audioDecode", 2, nx_audio_decode),
	JS_CFUNC_DEF("audioPlay", 7, nx_audio_play),
	JS_CFUNC_DEF("audioStop", 1, nx_audio_stop),
	JS_CFUNC_DEF("audioPause", 2, nx_audio_pause),
	JS_CFUNC_DEF("audioSetVolume", 2, nx_audio_set_volume),
	JS_CFUNC_DEF("audioSetPitch", 2, nx_audio_set_pitch),
	JS_CFUNC_DEF("audioUpdate", 0, nx_audio_update),
	JS_CFUNC_DEF("audioGetPlayedSamples", 1, nx_audio_get_played_samples),
	JS_CFUNC_DEF("audioAllocVoice", 0, nx_audio_alloc_voice),
	JS_CFUNC_DEF("audioFreeVoice", 1, nx_audio_free_voice),
	JS_CFUNC_DEF("audioIsPlaying", 1, nx_audio_is_playing),
};

void nx_init_audio(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
