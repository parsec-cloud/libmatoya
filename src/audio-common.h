#pragma once

#include <math.h>
#include <stdint.h>

#include "matoya.h"

struct audio_common {
	MTY_AudioFormat format;

	// Values derived from `format`
	struct {
		uint32_t sample_size; ///< Size in bytes of 1 sample of audio from 1 channel
		uint32_t frame_size;  ///< Size in bytes of 1 frame of audio, i.e. 1 sample of audio from each and every channel combined
		uint32_t buffer_size; ///< Size in bytes of a 1-second buffer
		uint32_t min_buffer;  ///< Minimum number of frames of audio that must be enqueued before playback
		uint32_t max_buffer;  ///< Maximum number of frames of audio that can be enqueued for playback
	} computed;
};

static void audio_common_init(struct audio_common *ctx, MTY_AudioFormat fmt, uint32_t min_buffer_ms,
	uint32_t max_buffer_ms)
{
	ctx->format = fmt;

	ctx->computed.sample_size = fmt.sampleFormat == MTY_AUDIO_SAMPLE_FORMAT_FLOAT
		? sizeof(float) : sizeof(int16_t);
	ctx->computed.frame_size = fmt.channels * ctx->computed.sample_size;
	ctx->computed.buffer_size = fmt.sampleRate * ctx->computed.frame_size;

	uint32_t samples_per_ms = lrintf(fmt.sampleRate / 1000.0f);
	ctx->computed.min_buffer = min_buffer_ms * samples_per_ms;
	ctx->computed.max_buffer = max_buffer_ms * samples_per_ms;
}
