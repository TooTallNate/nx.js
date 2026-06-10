// Portable ffmpeg media decode pipeline. See media-decoder.h for the
// architecture and threading contract. No V8 / libnx — compiled into both the
// device runtime and the host nxjs-test binary.
#include "media-decoder.h"
#include "audio-graph.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <math.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

namespace {

constexpr int RING_SLOTS = 3;
// Present a frame when its PTS is within this much of the clock (one frame of
// slack at 24 fps is ~41 ms; this is just sub-frame jitter tolerance).
constexpr double PRESENT_EPSILON = 0.001;
// Snap the wall clock to the audio clock when they drift further than this.
constexpr double AV_RESYNC_THRESHOLD = 0.05;

struct video_slot {
	uint8_t *bgra = nullptr;
	double pts = 0;
};

} // namespace

struct nx_media {
	// ---- IO (file or memory) ----
	FILE *file = nullptr;
	int64_t file_size = 0;
	const uint8_t *mem = nullptr;
	size_t mem_size = 0;
	size_t mem_pos = 0;

	// ---- ffmpeg ----
	AVIOContext *avio = nullptr;
	AVFormatContext *fmt = nullptr;
	AVCodecContext *vctx = nullptr;
	AVCodecContext *actx = nullptr;
	int vstream = -1;
	int astream = -1;
	SwsContext *sws = nullptr;
	SwrContext *swr = nullptr;

	// ---- metadata ----
	int width = 0;
	int height = 0;
	double duration = 0;

	// ---- video presentation ring (SPSC: decode thread -> main thread) ----
	video_slot slots[RING_SLOTS];
	std::atomic<uint64_t> vwrite{0};
	std::atomic<uint64_t> vread{0};

	// ---- audio output ----
	nx_audio_node *audio_node = nullptr;
	double audio_out_rate = 48000;
	std::vector<float> audio_scratch;
	// Mapping from the stream node's consumed-frame counter to media time:
	// audio_time(consumed) = audio_pts_base + (consumed - audio_base) / rate.
	std::atomic<bool> audio_clock_valid{false};
	std::atomic<double> audio_pts_base{0};
	std::atomic<uint64_t> audio_base{0};

	// ---- clock (main thread only) ----
	double clock_base = 0;
	std::chrono::steady_clock::time_point clock_anchor;
	bool clock_running = false;

	// ---- presentation quality counters (main thread only) ----
	uint64_t presented_frames = 0;
	uint64_t dropped_frames = 0;

	// ---- control ----
	std::mutex ctl_mutex;
	std::condition_variable ctl_cv;
	std::thread thread;
	std::atomic<bool> quit{false};
	std::atomic<bool> playing{false};
	std::atomic<bool> looping{false};
	std::atomic<bool> seek_requested{false};
	std::atomic<double> seek_target{0};
	std::atomic<bool> seeking{false};
	std::atomic<bool> eof{false};
	std::atomic<bool> fatal{false};
	char error_buf[256] = {};
	// Loop playback runs in a monotonic PTS domain: each wrap adds the media
	// duration so frame/audio PTS keep increasing past the clock.
	double loop_offset = 0; // decode thread only
};

namespace {

// ---------------------------------------------------------------------------
// Custom AVIO (stdio file or memory buffer)
// ---------------------------------------------------------------------------

int avio_read_cb(void *opaque, uint8_t *buf, int n) {
	nx_media *m = static_cast<nx_media *>(opaque);
	if (m->file) {
		size_t r = fread(buf, 1, (size_t)n, m->file);
		return r > 0 ? (int)r : AVERROR_EOF;
	}
	size_t rem = m->mem_size - m->mem_pos;
	if (rem == 0)
		return AVERROR_EOF;
	size_t r = rem < (size_t)n ? rem : (size_t)n;
	memcpy(buf, m->mem + m->mem_pos, r);
	m->mem_pos += r;
	return (int)r;
}

int64_t avio_seek_cb(void *opaque, int64_t offset, int whence) {
	nx_media *m = static_cast<nx_media *>(opaque);
	if (whence & AVSEEK_SIZE)
		return m->file ? m->file_size : (int64_t)m->mem_size;
	whence &= ~AVSEEK_FORCE;
	if (m->file) {
		if (fseek(m->file, (long)offset, whence) != 0)
			return -1;
		return (int64_t)ftell(m->file);
	}
	int64_t base = whence == SEEK_CUR    ? (int64_t)m->mem_pos
	               : whence == SEEK_END ? (int64_t)m->mem_size
	                                    : 0;
	int64_t pos = base + offset;
	if (pos < 0 || pos > (int64_t)m->mem_size)
		return -1;
	m->mem_pos = (size_t)pos;
	return pos;
}

// ---------------------------------------------------------------------------
// Decode thread
// ---------------------------------------------------------------------------

void set_fatal(nx_media *m, const char *what, int averr) {
	if (m->fatal.load())
		return;
	char detail[128] = {};
	if (averr != 0)
		av_strerror(averr, detail, sizeof(detail));
	snprintf(m->error_buf, sizeof(m->error_buf), "%s%s%s", what,
	         averr ? ": " : "", detail);
	m->fatal.store(true);
}

double frame_pts_seconds(nx_media *m, AVFrame *frame, int stream_index) {
	int64_t ts = frame->best_effort_timestamp;
	if (ts == AV_NOPTS_VALUE)
		ts = frame->pts;
	if (ts == AV_NOPTS_VALUE)
		return 0;
	return ts * av_q2d(m->fmt->streams[stream_index]->time_base);
}

// Sleep briefly, returning false if the thread should abandon its current
// blocking operation (quit or a pending seek).
bool decode_wait(nx_media *m) {
	if (m->quit.load() || m->seek_requested.load())
		return false;
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	return true;
}

// Blocks until a ring slot is free, then converts `frame` to BGRA into it.
// Returns false if interrupted (quit/seek).
bool enqueue_video(nx_media *m, AVFrame *frame, double pts) {
	while (m->vwrite.load(std::memory_order_relaxed) -
	           m->vread.load(std::memory_order_acquire) >=
	       RING_SLOTS) {
		if (!decode_wait(m))
			return false;
	}
	uint64_t w = m->vwrite.load(std::memory_order_relaxed);
	video_slot *slot = &m->slots[w % RING_SLOTS];

	m->sws = sws_getCachedContext(m->sws, frame->width, frame->height,
	                              (AVPixelFormat)frame->format, m->width,
	                              m->height, AV_PIX_FMT_BGRA, SWS_BILINEAR,
	                              NULL, NULL, NULL);
	if (!m->sws) {
		set_fatal(m, "failed to create scaler", 0);
		return false;
	}
	uint8_t *dst[4] = {slot->bgra, NULL, NULL, NULL};
	int dst_stride[4] = {m->width * 4, 0, 0, 0};
	sws_scale(m->sws, frame->data, frame->linesize, 0, frame->height, dst,
	          dst_stride);
	slot->pts = pts;
	m->vwrite.store(w + 1, std::memory_order_release);
	return true;
}

// Resample an audio frame to interleaved stereo f32 at the output rate and
// push it into the stream node (blocking on backpressure). Returns false if
// interrupted.
bool enqueue_audio(nx_media *m, AVFrame *frame, double pts) {
	if (!m->swr) {
		AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
		int ret = swr_alloc_set_opts2(
		    &m->swr, &out_layout, AV_SAMPLE_FMT_FLT,
		    (int)m->audio_out_rate, &frame->ch_layout,
		    (AVSampleFormat)frame->format, frame->sample_rate, 0, NULL);
		if (ret < 0 || swr_init(m->swr) < 0) {
			set_fatal(m, "failed to create resampler", ret);
			return false;
		}
	}
	int64_t max_out = av_rescale_rnd(
	    swr_get_delay(m->swr, frame->sample_rate) + frame->nb_samples,
	    (int64_t)m->audio_out_rate, frame->sample_rate, AV_ROUND_UP);
	m->audio_scratch.resize((size_t)max_out * 2);
	uint8_t *out_ptr = (uint8_t *)m->audio_scratch.data();
	int got = swr_convert(m->swr, &out_ptr, (int)max_out,
	                      (const uint8_t **)frame->extended_data,
	                      frame->nb_samples);
	if (got <= 0)
		return true;

	if (!m->audio_clock_valid.load(std::memory_order_relaxed)) {
		// First audio after open/seek: anchor the audio clock at this
		// frame's PTS and the node's current write position.
		m->audio_base.store(
		    m->audio_node->stream_write_pos.load(std::memory_order_relaxed),
		    std::memory_order_relaxed);
		m->audio_pts_base.store(pts, std::memory_order_relaxed);
		m->audio_clock_valid.store(true, std::memory_order_release);
	}

	const float *src = m->audio_scratch.data();
	uint32_t remaining = (uint32_t)got;
	while (remaining > 0) {
		uint32_t wrote =
		    nx_audio_stream_write(m->audio_node, src, remaining);
		src += (size_t)wrote * 2;
		remaining -= wrote;
		if (remaining > 0 && !decode_wait(m))
			return false;
	}
	return true;
}

// Receive all pending frames from a codec. `seek_drop_until` (media seconds,
// or <0) drops frames decoded while converging on a seek target. Returns
// false if interrupted.
bool receive_frames(nx_media *m, AVCodecContext *ctx, int stream_index,
                    AVFrame *frame, double seek_drop_until) {
	while (true) {
		int ret = avcodec_receive_frame(ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return true;
		if (ret < 0) {
			set_fatal(m, "decode error", ret);
			return false;
		}
		double pts = frame_pts_seconds(m, frame, stream_index) +
		             m->loop_offset;
		bool is_video = stream_index == m->vstream;
		if (seek_drop_until >= 0 && pts < seek_drop_until - 0.005) {
			av_frame_unref(frame);
			continue;
		}
		bool ok = is_video ? enqueue_video(m, frame, pts)
		                   : enqueue_audio(m, frame, pts);
		if (is_video && ok && m->seeking.load(std::memory_order_relaxed) &&
		    seek_drop_until >= 0) {
			// First on-target frame after a seek: the seek is complete.
			m->seeking.store(false, std::memory_order_release);
		}
		av_frame_unref(frame);
		if (!ok)
			return false;
	}
}

// Drain both decoders at EOF. Returns false if interrupted.
bool drain_decoders(nx_media *m, AVFrame *frame, double seek_drop_until) {
	if (m->vctx) {
		avcodec_send_packet(m->vctx, NULL);
		if (!receive_frames(m, m->vctx, m->vstream, frame, seek_drop_until))
			return false;
	}
	if (m->actx && m->audio_node) {
		avcodec_send_packet(m->actx, NULL);
		if (!receive_frames(m, m->actx, m->astream, frame, seek_drop_until))
			return false;
	}
	return true;
}

// Flush codec + demuxer state and seek the container. Caller is the decode
// thread. `to_seconds` is in the un-looped media domain.
bool container_seek(nx_media *m, double to_seconds) {
	if (m->vctx)
		avcodec_flush_buffers(m->vctx);
	if (m->actx)
		avcodec_flush_buffers(m->actx);
	if (m->swr) {
		swr_free(&m->swr); // drop resampler delay state
	}
	int64_t ts = (int64_t)(to_seconds * AV_TIME_BASE);
	int ret = av_seek_frame(m->fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		// Fall back to a byte-position rewind for streams without an index.
		ret = av_seek_frame(m->fmt, -1, 0,
		                    AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_BYTE);
	}
	return ret >= 0;
}

// Handle a user-requested seek (decode thread).
void do_seek(nx_media *m, double *seek_drop_until) {
	double target = m->seek_target.load();
	// Discard everything queued: the main thread is guaranteed not to touch
	// the video ring while `seeking` is true.
	m->vread.store(0, std::memory_order_relaxed);
	m->vwrite.store(0, std::memory_order_relaxed);
	if (m->audio_node) {
		nx_audio_stream_flush(m->audio_node);
		m->audio_clock_valid.store(false, std::memory_order_relaxed);
	}
	m->loop_offset = 0;
	m->eof.store(false);
	if (!container_seek(m, target)) {
		set_fatal(m, "seek failed", 0);
		m->seeking.store(false);
		return;
	}
	*seek_drop_until = target;
	if (m->vstream < 0) {
		// No video track: nothing will flip `seeking` in receive_frames.
		m->seeking.store(false);
	}
}

void decode_thread_main(nx_media *m) {
	AVPacket *pkt = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	if (!pkt || !frame) {
		set_fatal(m, "out of memory", 0);
		av_packet_free(&pkt);
		av_frame_free(&frame);
		return;
	}
	double seek_drop_until = -1;

	while (!m->quit.load()) {
		if (m->seek_requested.exchange(false)) {
			do_seek(m, &seek_drop_until);
			continue;
		}
		if (m->fatal.load() || m->eof.load()) {
			// Parked: wait for a command (seek/quit, or loop handled below).
			std::unique_lock<std::mutex> lock(m->ctl_mutex);
			m->ctl_cv.wait_for(lock, std::chrono::milliseconds(100));
			continue;
		}

		int ret = av_read_frame(m->fmt, pkt);
		if (ret == AVERROR_EOF) {
			if (!drain_decoders(m, frame, seek_drop_until))
				continue;
			seek_drop_until = -1;
			if (m->looping.load()) {
				// Gapless loop: wrap the container, keep PTS monotonic.
				double advance = m->duration > 0 ? m->duration : 0;
				if (!container_seek(m, 0)) {
					set_fatal(m, "loop seek failed", 0);
					continue;
				}
				m->loop_offset += advance;
				continue;
			}
			m->eof.store(true);
			continue;
		}
		if (ret < 0) {
			set_fatal(m, "demux error", ret);
			av_packet_unref(pkt);
			continue;
		}

		if (pkt->stream_index == m->vstream && m->vctx) {
			ret = avcodec_send_packet(m->vctx, pkt);
			if (ret == 0 || ret == AVERROR(EAGAIN)) {
				if (receive_frames(m, m->vctx, m->vstream, frame,
				                   seek_drop_until) &&
				    seek_drop_until >= 0 && !m->seeking.load()) {
					seek_drop_until = -1;
				}
			}
		} else if (pkt->stream_index == m->astream && m->actx &&
		           m->audio_node) {
			ret = avcodec_send_packet(m->actx, pkt);
			if (ret == 0 || ret == AVERROR(EAGAIN)) {
				receive_frames(m, m->actx, m->astream, frame,
				               seek_drop_until);
			}
		}
		av_packet_unref(pkt);
	}

	av_packet_free(&pkt);
	av_frame_free(&frame);
}

// ---------------------------------------------------------------------------
// Clock (main thread)
// ---------------------------------------------------------------------------

double clock_now(nx_media *m) {
	double t = m->clock_base;
	if (m->clock_running) {
		t += std::chrono::duration<double>(
		         std::chrono::steady_clock::now() - m->clock_anchor)
		         .count();
	}
	// Slave to the audio clock when audio is flowing.
	if (m->clock_running && m->audio_node &&
	    m->audio_clock_valid.load(std::memory_order_acquire)) {
		uint64_t consumed = nx_audio_stream_consumed(m->audio_node);
		uint64_t base = m->audio_base.load(std::memory_order_relaxed);
		if (consumed > base) {
			double at = m->audio_pts_base.load(std::memory_order_relaxed) +
			            (double)(consumed - base) / m->audio_out_rate;
			if (fabs(at - t) > AV_RESYNC_THRESHOLD) {
				m->clock_base = at;
				m->clock_anchor = std::chrono::steady_clock::now();
				t = at;
			}
		}
	}
	return t;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

nx_media_t *nx_media_open(const char *path, const uint8_t *mem,
                          size_t mem_size, char *errbuf, size_t errbuf_size) {
	nx_media *m = new nx_media();
	int ret = 0;
	const AVCodec *vcodec = NULL;
	const AVCodec *acodec = NULL;
	unsigned char *avio_buf = NULL;

	if (path) {
		m->file = fopen(path, "rb");
		if (!m->file) {
			snprintf(errbuf, errbuf_size, "failed to open file");
			goto fail;
		}
		fseek(m->file, 0, SEEK_END);
		m->file_size = (int64_t)ftell(m->file);
		fseek(m->file, 0, SEEK_SET);
	} else {
		m->mem = mem;
		m->mem_size = mem_size;
	}

	avio_buf = (unsigned char *)av_malloc(65536);
	if (!avio_buf) {
		snprintf(errbuf, errbuf_size, "out of memory");
		goto fail;
	}
	m->avio = avio_alloc_context(avio_buf, 65536, 0, m, avio_read_cb, NULL,
	                             avio_seek_cb);
	if (!m->avio) {
		av_free(avio_buf);
		snprintf(errbuf, errbuf_size, "out of memory");
		goto fail;
	}
	m->fmt = avformat_alloc_context();
	if (!m->fmt) {
		snprintf(errbuf, errbuf_size, "out of memory");
		goto fail;
	}
	m->fmt->pb = m->avio;

	ret = avformat_open_input(&m->fmt, NULL, NULL, NULL);
	if (ret < 0) {
		av_strerror(ret, errbuf, errbuf_size);
		goto fail;
	}
	ret = avformat_find_stream_info(m->fmt, NULL);
	if (ret < 0) {
		av_strerror(ret, errbuf, errbuf_size);
		goto fail;
	}

	m->vstream = av_find_best_stream(m->fmt, AVMEDIA_TYPE_VIDEO, -1, -1,
	                                 &vcodec, 0);
	m->astream = av_find_best_stream(m->fmt, AVMEDIA_TYPE_AUDIO, -1, -1,
	                                 &acodec, 0);
	if (m->vstream < 0 && m->astream < 0) {
		snprintf(errbuf, errbuf_size, "no playable streams");
		goto fail;
	}

	if (m->vstream >= 0) {
		AVCodecParameters *par = m->fmt->streams[m->vstream]->codecpar;
		m->vctx = avcodec_alloc_context3(vcodec);
		if (!m->vctx ||
		    avcodec_parameters_to_context(m->vctx, par) < 0) {
			snprintf(errbuf, errbuf_size, "failed to set up video decoder");
			goto fail;
		}
		// Two decode threads: leaves the main JS thread + audio render
		// thread breathing room on the Switch's three usable cores.
		m->vctx->thread_count = 2;
		ret = avcodec_open2(m->vctx, vcodec, NULL);
		if (ret < 0) {
			av_strerror(ret, errbuf, errbuf_size);
			goto fail;
		}
		m->width = par->width;
		m->height = par->height;
		if (m->width <= 0 || m->height <= 0 || m->width > 4096 ||
		    m->height > 4096) {
			snprintf(errbuf, errbuf_size, "unsupported video dimensions");
			goto fail;
		}
		for (int i = 0; i < RING_SLOTS; i++) {
			m->slots[i].bgra =
			    (uint8_t *)malloc((size_t)m->width * m->height * 4);
			if (!m->slots[i].bgra) {
				snprintf(errbuf, errbuf_size, "out of memory");
				goto fail;
			}
		}
	}

	if (m->astream >= 0) {
		AVCodecParameters *par = m->fmt->streams[m->astream]->codecpar;
		m->actx = avcodec_alloc_context3(acodec);
		if (!m->actx ||
		    avcodec_parameters_to_context(m->actx, par) < 0 ||
		    avcodec_open2(m->actx, acodec, NULL) < 0) {
			// Audio is non-fatal: play the video silently.
			if (m->actx)
				avcodec_free_context(&m->actx);
			m->astream = -1;
		}
	}

	if (m->fmt->duration != AV_NOPTS_VALUE && m->fmt->duration > 0)
		m->duration = (double)m->fmt->duration / AV_TIME_BASE;

	m->thread = std::thread(decode_thread_main, m);
	return m;

fail:
	nx_media_destroy(m);
	return NULL;
}

int nx_media_width(nx_media_t *m) { return m->width; }
int nx_media_height(nx_media_t *m) { return m->height; }
double nx_media_duration(nx_media_t *m) { return m->duration; }
bool nx_media_has_audio(nx_media_t *m) { return m->astream >= 0; }
bool nx_media_has_video(nx_media_t *m) { return m->vstream >= 0; }

void nx_media_set_audio_node(nx_media_t *m, nx_audio_node *node,
                             double sample_rate) {
	m->audio_node = node;
	m->audio_out_rate = sample_rate;
}

void nx_media_play(nx_media_t *m) {
	if (m->playing.load())
		return;
	m->clock_anchor = std::chrono::steady_clock::now();
	m->clock_running = true;
	m->playing.store(true);
	if (m->audio_node)
		nx_audio_stream_set_playing(m->audio_node, true);
	m->ctl_cv.notify_all();
}

void nx_media_pause(nx_media_t *m) {
	if (!m->playing.load())
		return;
	m->clock_base = clock_now(m);
	m->clock_running = false;
	m->playing.store(false);
	if (m->audio_node)
		nx_audio_stream_set_playing(m->audio_node, false);
}

void nx_media_seek(nx_media_t *m, double seconds) {
	if (seconds < 0)
		seconds = 0;
	if (m->duration > 0 && seconds > m->duration)
		seconds = m->duration;
	// Stop presentation/clock reads of the ring first.
	m->seeking.store(true, std::memory_order_release);
	m->clock_base = seconds;
	m->clock_anchor = std::chrono::steady_clock::now();
	m->seek_target.store(seconds);
	m->seek_requested.store(true);
	m->ctl_cv.notify_all();
}

void nx_media_set_loop(nx_media_t *m, bool loop) {
	m->looping.store(loop);
	m->ctl_cv.notify_all();
}

bool nx_media_present(nx_media_t *m, uint8_t **buffer_inout) {
	if (m->seeking.load(std::memory_order_acquire) || m->fatal.load())
		return false;
	double t = clock_now(m);
	uint64_t r = m->vread.load(std::memory_order_relaxed);
	uint64_t w = m->vwrite.load(std::memory_order_acquire);
	int64_t candidate = -1;
	for (uint64_t i = r; i < w; i++) {
		if (m->slots[i % RING_SLOTS].pts <= t + PRESENT_EPSILON)
			candidate = (int64_t)i;
		else
			break;
	}
	if (candidate < 0)
		return false;
	video_slot *slot = &m->slots[candidate % RING_SLOTS];
	uint8_t *prev = *buffer_inout;
	*buffer_inout = slot->bgra;
	slot->bgra = prev;
	m->presented_frames++;
	m->dropped_frames += (uint64_t)candidate - r;
	m->vread.store((uint64_t)candidate + 1, std::memory_order_release);
	return true;
}

uint64_t nx_media_presented_frames(nx_media_t *m) {
	return m->presented_frames;
}

uint64_t nx_media_dropped_frames(nx_media_t *m) { return m->dropped_frames; }

double nx_media_current_time(nx_media_t *m) {
	if (m->seeking.load())
		return m->seek_target.load();
	double t = clock_now(m);
	if (m->duration > 0) {
		if (m->looping.load())
			t = fmod(t, m->duration);
		else if (t > m->duration)
			t = m->duration;
	}
	return t < 0 ? 0 : t;
}

uint32_t nx_media_buffered_frames(nx_media_t *m) {
	return (uint32_t)(m->vwrite.load(std::memory_order_acquire) -
	                  m->vread.load(std::memory_order_relaxed));
}

bool nx_media_ended(nx_media_t *m) {
	if (!m->eof.load() || m->looping.load())
		return false;
	if (m->vwrite.load(std::memory_order_acquire) !=
	    m->vread.load(std::memory_order_relaxed))
		return false;
	if (m->audio_node && m->audio_clock_valid.load()) {
		uint64_t written =
		    m->audio_node->stream_write_pos.load(std::memory_order_acquire);
		if (nx_audio_stream_consumed(m->audio_node) < written)
			return false;
	}
	return true;
}

bool nx_media_seeking(nx_media_t *m) { return m->seeking.load(); }

const char *nx_media_error(nx_media_t *m) {
	return m->fatal.load() ? m->error_buf : NULL;
}

// ---------------------------------------------------------------------------
// Whole-file audio decode (decodeAudioData / Audio element)
// ---------------------------------------------------------------------------

namespace {

struct mem_reader {
	const uint8_t *data;
	size_t size;
	size_t pos;
};

int mem_read_cb(void *opaque, uint8_t *buf, int n) {
	mem_reader *r = static_cast<mem_reader *>(opaque);
	size_t rem = r->size - r->pos;
	if (rem == 0)
		return AVERROR_EOF;
	size_t count = rem < (size_t)n ? rem : (size_t)n;
	memcpy(buf, r->data + r->pos, count);
	r->pos += count;
	return (int)count;
}

int64_t mem_seek_cb(void *opaque, int64_t offset, int whence) {
	mem_reader *r = static_cast<mem_reader *>(opaque);
	if (whence & AVSEEK_SIZE)
		return (int64_t)r->size;
	whence &= ~AVSEEK_FORCE;
	int64_t base = whence == SEEK_CUR    ? (int64_t)r->pos
	               : whence == SEEK_END ? (int64_t)r->size
	                                    : 0;
	int64_t pos = base + offset;
	if (pos < 0 || pos > (int64_t)r->size)
		return -1;
	r->pos = (size_t)pos;
	return pos;
}

} // namespace

bool nx_media_decode_audio(const uint8_t *data, size_t size,
                           float *channels[NX_MEDIA_MAX_CHANNELS],
                           int *num_channels, uint32_t *length,
                           uint32_t *sample_rate, char *errbuf,
                           size_t errbuf_size) {
	mem_reader reader = {data, size, 0};
	AVIOContext *avio = NULL;
	AVFormatContext *fmt = NULL;
	AVCodecContext *ctx = NULL;
	SwrContext *swr = NULL;
	AVPacket *pkt = NULL;
	AVFrame *frame = NULL;
	const AVCodec *codec = NULL;
	int stream = -1;
	int ret = 0;
	int nch = 0;
	bool ok = false;
	// Planar accumulation buffers (resized as frames arrive).
	std::vector<std::vector<float>> acc;
	std::vector<float> scratch;

	unsigned char *avio_buf = (unsigned char *)av_malloc(65536);
	if (!avio_buf) {
		snprintf(errbuf, errbuf_size, "out of memory");
		goto done;
	}
	avio = avio_alloc_context(avio_buf, 65536, 0, &reader, mem_read_cb, NULL,
	                          mem_seek_cb);
	if (!avio) {
		av_free(avio_buf);
		snprintf(errbuf, errbuf_size, "out of memory");
		goto done;
	}
	fmt = avformat_alloc_context();
	if (!fmt) {
		snprintf(errbuf, errbuf_size, "out of memory");
		goto done;
	}
	fmt->pb = avio;
	ret = avformat_open_input(&fmt, NULL, NULL, NULL);
	if (ret < 0) {
		av_strerror(ret, errbuf, errbuf_size);
		goto done;
	}
	ret = avformat_find_stream_info(fmt, NULL);
	if (ret < 0) {
		av_strerror(ret, errbuf, errbuf_size);
		goto done;
	}
	stream = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
	if (stream < 0) {
		snprintf(errbuf, errbuf_size, "no audio stream found");
		goto done;
	}
	ctx = avcodec_alloc_context3(codec);
	if (!ctx ||
	    avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar) <
	        0 ||
	    avcodec_open2(ctx, codec, NULL) < 0) {
		snprintf(errbuf, errbuf_size, "failed to open audio decoder");
		goto done;
	}
	pkt = av_packet_alloc();
	frame = av_frame_alloc();
	if (!pkt || !frame) {
		snprintf(errbuf, errbuf_size, "out of memory");
		goto done;
	}

	while (true) {
		ret = av_read_frame(fmt, pkt);
		bool at_eof = ret == AVERROR_EOF;
		if (ret < 0 && !at_eof) {
			av_strerror(ret, errbuf, errbuf_size);
			goto done;
		}
		if (!at_eof && pkt->stream_index != stream) {
			av_packet_unref(pkt);
			continue;
		}
		ret = avcodec_send_packet(ctx, at_eof ? NULL : pkt);
		if (!at_eof)
			av_packet_unref(pkt);
		if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
			av_strerror(ret, errbuf, errbuf_size);
			goto done;
		}
		while (true) {
			ret = avcodec_receive_frame(ctx, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				break;
			if (ret < 0) {
				av_strerror(ret, errbuf, errbuf_size);
				goto done;
			}
			if (nch == 0) {
				nch = frame->ch_layout.nb_channels;
				if (nch > NX_MEDIA_MAX_CHANNELS)
					nch = NX_MEDIA_MAX_CHANNELS;
				if (nch <= 0) {
					snprintf(errbuf, errbuf_size, "no audio channels");
					goto done;
				}
				*sample_rate = (uint32_t)frame->sample_rate;
				acc.resize((size_t)nch);
				// Convert to interleaved f32 at the native rate/layout —
				// deinterleave below (swr output layout = input layout).
				ret = swr_alloc_set_opts2(
				    &swr, &frame->ch_layout, AV_SAMPLE_FMT_FLT,
				    frame->sample_rate, &frame->ch_layout,
				    (AVSampleFormat)frame->format, frame->sample_rate, 0,
				    NULL);
				if (ret < 0 || swr_init(swr) < 0) {
					snprintf(errbuf, errbuf_size,
					         "failed to create resampler");
					goto done;
				}
			}
			int src_nch = frame->ch_layout.nb_channels;
			scratch.resize((size_t)frame->nb_samples * src_nch);
			uint8_t *out_ptr = (uint8_t *)scratch.data();
			int got = swr_convert(swr, &out_ptr, frame->nb_samples,
			                      (const uint8_t **)frame->extended_data,
			                      frame->nb_samples);
			if (got > 0) {
				for (int c = 0; c < nch; c++) {
					std::vector<float> &dst = acc[(size_t)c];
					size_t off = dst.size();
					dst.resize(off + (size_t)got);
					for (int i = 0; i < got; i++)
						dst[off + i] = scratch[(size_t)i * src_nch + c];
				}
			}
			av_frame_unref(frame);
		}
		if (at_eof)
			break;
	}

	if (nch == 0 || acc[0].empty()) {
		snprintf(errbuf, errbuf_size, "audio file contains no audio data");
		goto done;
	}
	*num_channels = nch;
	*length = (uint32_t)acc[0].size();
	for (int c = 0; c < nch; c++) {
		channels[c] = (float *)malloc((size_t)*length * sizeof(float));
		if (!channels[c]) {
			for (int j = 0; j < c; j++) {
				free(channels[j]);
				channels[j] = nullptr;
			}
			snprintf(errbuf, errbuf_size, "out of memory");
			goto done;
		}
		size_t have = acc[(size_t)c].size();
		size_t want = (size_t)*length;
		memcpy(channels[c], acc[(size_t)c].data(),
		       (have < want ? have : want) * sizeof(float));
	}
	ok = true;

done:
	if (pkt)
		av_packet_free(&pkt);
	if (frame)
		av_frame_free(&frame);
	if (swr)
		swr_free(&swr);
	if (ctx)
		avcodec_free_context(&ctx);
	if (fmt)
		avformat_close_input(&fmt);
	if (avio) {
		av_freep(&avio->buffer);
		avio_context_free(&avio);
	}
	return ok;
}

void nx_media_destroy(nx_media_t *m) {
	m->quit.store(true);
	m->ctl_cv.notify_all();
	if (m->thread.joinable())
		m->thread.join();
	if (m->audio_node)
		nx_audio_stream_set_playing(m->audio_node, false);
	if (m->sws)
		sws_freeContext(m->sws);
	if (m->swr)
		swr_free(&m->swr);
	if (m->vctx)
		avcodec_free_context(&m->vctx);
	if (m->actx)
		avcodec_free_context(&m->actx);
	if (m->fmt)
		avformat_close_input(&m->fmt);
	if (m->avio) {
		av_freep(&m->avio->buffer);
		avio_context_free(&m->avio);
	}
	for (int i = 0; i < RING_SLOTS; i++)
		free(m->slots[i].bgra);
	if (m->file)
		fclose(m->file);
	delete m;
}
