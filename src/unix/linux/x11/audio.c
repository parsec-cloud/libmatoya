// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <math.h>

#include "dl/libasound.h"

#include "audio-common.h"

struct MTY_Audio {
	struct audio_common cmn;
	snd_pcm_t *pcm;
	bool playing;
	uint8_t *buf;
	size_t pos;
};

MTY_Audio *MTY_AudioCreate(MTY_AudioFormat format, uint32_t minBuffer,
	uint32_t maxBuffer, const char *deviceID, bool fallback)
{
	if (!libasound_global_init())
		return NULL;

	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	audio_common_init(&ctx->cmn, format, minBuffer, maxBuffer);

	bool r = true;

	int32_t e = snd_pcm_open(&ctx->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (e != 0) {
		MTY_Log("'snd_pcm_open' failed with error %d", e);
		r = false;
		goto except;
	}

	snd_pcm_hw_params_t *params = NULL;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(ctx->pcm, params);

	snd_pcm_hw_params_set_access(ctx->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(ctx->pcm, params, format.sampleFormat == MTY_AUDIO_SAMPLE_FORMAT_FLOAT
		? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16);
	snd_pcm_hw_params_set_channels(ctx->pcm, params, format.channels);
	// XXX: Channel config for ALSA can't be specified via the opaque `format.channelMask`
	// Instead, an explicit channel mapping array is required. To be implemented in the future.
	// See `snd_pcm_set_chmap`:
	// 1. https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html#ga60ee7d2c2555e21dbc844a1b73839085
	// 2. https://gist.github.com/raydudu/5590a196b9446c709c58a03eff1f38bc
	snd_pcm_hw_params_set_rate(ctx->pcm, params, format.sampleRate, 0);
	snd_pcm_hw_params(ctx->pcm, params);
	snd_pcm_nonblock(ctx->pcm, 1);

	ctx->buf = MTY_Alloc(ctx->cmn.computed.buffer_size, 1);

	except:

	if (!r)
		MTY_AudioDestroy(&ctx);

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	if (ctx->pcm)
		snd_pcm_close(ctx->pcm);

	MTY_Free(ctx->buf);
	MTY_Free(ctx);
	*audio = NULL;
}

static uint32_t audio_get_queued_frames(MTY_Audio *ctx)
{
	uint32_t queued = ctx->pos / ctx->cmn.computed.frame_size;

	if (ctx->playing) {
		snd_pcm_status_t *status = NULL;
		snd_pcm_status_alloca(&status);

		if (snd_pcm_status(ctx->pcm, status) >= 0) {
			snd_pcm_uframes_t avail = snd_pcm_status_get_avail(status);
			snd_pcm_uframes_t avail_max = snd_pcm_status_get_avail_max(status);

			if (avail_max > 0)
				queued += (int32_t) avail_max - (int32_t) avail;
		}
	}

	return queued;
}

static void audio_play(MTY_Audio *ctx)
{
	if (!ctx->playing) {
		snd_pcm_prepare(ctx->pcm);
		ctx->playing = true;
	}
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	ctx->playing = false;
	ctx->pos = 0;
}

uint32_t MTY_AudioGetQueued(MTY_Audio *ctx)
{
	return lrint((float) audio_get_queued_frames(ctx) / ((float) ctx->cmn.format.sampleRate / 1000.0f));
}

void MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count)
{
	size_t size = count * ctx->cmn.computed.frame_size;

	uint32_t queued = audio_get_queued_frames(ctx);

	// Stop playing and flush if we've exceeded the maximum buffer or underrun
	if (ctx->playing && (queued > ctx->cmn.computed.max_buffer || queued == 0))
		MTY_AudioReset(ctx);

	if (ctx->pos + size <= ctx->cmn.computed.buffer_size) {
		memcpy(ctx->buf + ctx->pos, frames, count * ctx->cmn.computed.frame_size);
		ctx->pos += size;
	}

	// Begin playing again when the minimum buffer has been reached
	if (!ctx->playing && queued + count >= ctx->cmn.computed.min_buffer)
		audio_play(ctx);

	if (ctx->playing) {
		int32_t e = snd_pcm_writei(ctx->pcm, ctx->buf, ctx->pos / ctx->cmn.computed.frame_size);

		if (e >= 0) {
			ctx->pos = 0;

		} else if (e == -EPIPE) {
			MTY_AudioReset(ctx);
		}
	}
}
