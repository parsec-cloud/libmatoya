// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

#include "audio-common.h"

#define AUDIO_BUFS        64

#define AUDIO_HW_ERROR(e) ((e) != kAudioHardwareNoError)
#define AUDIO_SV_ERROR(e) ((e) != kAudioServicesNoError)

struct MTY_Audio {
	struct audio_common cmn;
	AudioQueueRef q;
	AudioQueueBufferRef audio_buf[AUDIO_BUFS];
	MTY_Atomic32 in_use[AUDIO_BUFS];
	bool playing;
};

static void audio_queue_callback(void *opaque, AudioQueueRef q, AudioQueueBufferRef buf)
{
	MTY_Audio *ctx = opaque;

	MTY_Atomic32Set(&ctx->in_use[(uintptr_t) buf->mUserData], 0);
	buf->mAudioDataByteSize = 0;
}

static OSStatus audio_object_get_device_uid(AudioObjectID device, AudioObjectPropertyScope prop_scope,
	AudioObjectPropertyElement prop_element, char **out_uid, CFStringRef *out_uid_cf)
{
	char *uid = NULL;
	CFStringRef uid_cf = NULL;

	AudioObjectPropertyAddress propAddr = {
		.mSelector = kAudioDevicePropertyDeviceUID,
		.mScope = prop_scope,
		.mElement = prop_element,
	};

	UInt32 data_size = sizeof(CFStringRef);
	OSStatus e = AudioObjectGetPropertyData(device, &propAddr, 0, NULL, &data_size, &uid_cf);
	if (AUDIO_HW_ERROR(e))
		goto except;

	UInt32 prop_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(uid_cf), kCFStringEncodingUTF8);
	uid = calloc(1, prop_size + 1);
	if (!CFStringGetCString(uid_cf, uid, prop_size + 1, kCFStringEncodingUTF8)) {
		e = kAudioHardwareBadDeviceError;
		goto except;
	}

	// OK
	*out_uid = uid;
	*out_uid_cf = uid_cf;

	except:

	if (AUDIO_HW_ERROR(e)) {
		if (uid_cf)
			CFRelease(uid_cf);
		free(uid);
	}

	return e;
}

static OSStatus audio_device_create(MTY_Audio *ctx, const char *deviceID)
{
	AudioObjectID *selected_device = NULL;
	CFStringRef selected_device_uid = NULL;

	// Enumerate all output devices and identify the given device
	AudioObjectID *device_ids = NULL;

	bool default_dev = !deviceID || !deviceID[0];

	AudioObjectPropertyAddress propAddr = {
		.mSelector = default_dev ? kAudioHardwarePropertyDefaultOutputDevice : kAudioHardwarePropertyDevices,
		.mScope = kAudioObjectPropertyScopeOutput,
		.mElement = kAudioObjectPropertyElementWildcard,
	};

	UInt32 data_size = 0;
	OSStatus e = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propAddr, 0, NULL, &data_size);
	if (AUDIO_HW_ERROR(e))
		goto except;

	if (data_size == 0) {
		e = kAudioHardwareBadDeviceError;
		goto except;
	}

	device_ids = calloc(1, data_size);
	e = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propAddr, 0, NULL, &data_size, device_ids);
	if (AUDIO_HW_ERROR(e))
		goto except;

	uint32_t n = (uint32_t) (data_size / sizeof(AudioObjectID));

	if (default_dev) {
		if (n != 1) {
			e = kAudioHardwareBadDeviceError;
			goto except;
		}

		selected_device = device_ids;

	} else {
		// Goal: find the AudioObjectID of the device whose unique string ID matches the given `deviceID` parameter
		for (uint32_t i = 0; !selected_device && i < n; i++) {
			char *uid = NULL;
			CFStringRef uid_cf = NULL;

			if (AUDIO_HW_ERROR(audio_object_get_device_uid(device_ids[i],
				propAddr.mScope, propAddr.mElement, &uid, &uid_cf)))
				continue;

			if (!strcmp(deviceID, uid)) {
				selected_device = &device_ids[i];
				selected_device_uid = uid_cf;
				uid_cf = NULL;
			}

			if (uid_cf)
				CFRelease(uid_cf);

			free(uid);
		}
	}

	if (!selected_device) {
		e = kAudioHardwareBadDeviceError;
		goto except;
	}

	// Initialize the selected device

	AudioStreamBasicDescription format = {0};
	format.mSampleRate = ctx->cmn.format.sampleRate;
	format.mFormatID = kAudioFormatLinearPCM;
	format.mFormatFlags = (ctx->cmn.format.sampleFormat == MTY_AUDIO_SAMPLE_FORMAT_FLOAT ?
		kAudioFormatFlagIsFloat : kAudioFormatFlagIsSignedInteger) | kAudioFormatFlagIsPacked;
	format.mFramesPerPacket = 1;
	format.mChannelsPerFrame = ctx->cmn.format.channels;
	format.mBitsPerChannel = ctx->cmn.computed.sample_size * 8;
	format.mBytesPerPacket = ctx->cmn.computed.frame_size;
	format.mBytesPerFrame = format.mBytesPerPacket;

	// Create a new audio queue, which by default chooses the device's default device
	e = AudioQueueNewOutput(&format, audio_queue_callback, ctx, NULL, NULL, 0, &ctx->q);
	if (AUDIO_SV_ERROR(e)) {
		MTY_Log("'AudioQueueNewOutput' failed with error 0x%X", e);
		goto except;
	}

	// Change the audio queue to be associated with the selected audio device
	if (selected_device_uid) {
		e = AudioQueueSetProperty(ctx->q, kAudioQueueProperty_CurrentDevice,
			(const void *) &selected_device_uid, sizeof(CFStringRef));
		if (AUDIO_SV_ERROR(e)) {
			MTY_Log("'AudioQueueSetProperty(kAudioQueueProperty_CurrentDevice)' failed with error 0x%X", e);
			goto except;
		}
	}

	// Specify channel configuration
	if (ctx->cmn.format.channelMask) {
		AudioChannelLayout channel_layout = {
			.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap,
			.mChannelBitmap = ctx->cmn.format.channelMask, // Core Audio channel bitmap follows the spec that the WAVE format
			                                                // follows, so we can simply pass in the mask as-is
		};

		e = AudioQueueSetProperty(ctx->q, kAudioQueueProperty_ChannelLayout,
			(const void *) &channel_layout, sizeof(AudioChannelLayout));
		if (AUDIO_SV_ERROR(e)) {
			MTY_Log("'AudioQueueSetProperty(kAudioQueueProperty_ChannelLayout)' failed with error 0x%X", e);
			goto except;
		}
	}

	for (int32_t x = 0; x < AUDIO_BUFS; x++) {
		e = AudioQueueAllocateBuffer(ctx->q, ctx->cmn.computed.buffer_size, &ctx->audio_buf[x]);
		if (AUDIO_SV_ERROR(e)) {
			MTY_Log("'AudioQueueAllocateBuffer' failed with error 0x%X", e);
			goto except;
		}
	}

	except:

	if (selected_device_uid)
		CFRelease(selected_device_uid);

	free(device_ids);

	return e;
}

MTY_Audio *MTY_AudioCreate(MTY_AudioFormat format_in, uint32_t minBuffer, uint32_t maxBuffer,
	const char *deviceID, bool fallback)
{
	// TODO Should this use the current run loop rather than internal threading?

	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	audio_common_init(&ctx->cmn, format_in, minBuffer, maxBuffer);

	OSStatus e = audio_device_create(ctx, deviceID);

	// Upon failure initializing the given device, try again with the system default device
	if (AUDIO_SV_ERROR(e) && deviceID && deviceID[0])
		e = audio_device_create(ctx, NULL);

	if (AUDIO_SV_ERROR(e))
		MTY_AudioDestroy(&ctx);

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	if (ctx->q) {
		OSStatus e = AudioQueueDispose(ctx->q, true);
		if (AUDIO_SV_ERROR(e))
			MTY_Log("'AudioQueueDispose' failed with error 0x%X", e);
	}

	MTY_Free(ctx);
	*audio = NULL;
}

static uint32_t audio_get_queued_frames(MTY_Audio *ctx)
{
	size_t queued = 0;

	for (uint8_t x = 0; x < AUDIO_BUFS; x++) {
		if (MTY_Atomic32Get(&ctx->in_use[x]) == 1) {
			AudioQueueBufferRef buf = ctx->audio_buf[x];
			queued += buf->mAudioDataByteSize;
		}
	}

	return queued / ctx->cmn.computed.frame_size;
}

static void audio_play(MTY_Audio *ctx)
{
	if (ctx->playing)
		return;

	if (!AUDIO_SV_ERROR(AudioQueueStart(ctx->q, NULL)))
		ctx->playing = true;
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	if (!ctx->playing)
		return;

	if (!AUDIO_SV_ERROR(AudioQueueStop(ctx->q, true)))
		ctx->playing = false;
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

	if (size <= ctx->cmn.computed.buffer_size) {
		for (uint8_t x = 0; x < AUDIO_BUFS; x++) {
			if (MTY_Atomic32Get(&ctx->in_use[x]) == 0) {
				AudioQueueBufferRef buf = ctx->audio_buf[x];

				memcpy(buf->mAudioData, frames, size);
				buf->mAudioDataByteSize = size;
				buf->mUserData = (void *) (uintptr_t) x;

				OSStatus e = AudioQueueEnqueueBuffer(ctx->q, buf, 0, NULL);
				if (!AUDIO_SV_ERROR(kAudioServicesNoError)) {
					MTY_Atomic32Set(&ctx->in_use[x], 1);

				} else {
					MTY_Log("'AudioQueueEnqueueBuffer' failed with error 0x%X", e);
				}
				break;
			}
		}

		// Begin playing again when the minimum buffer has been reached
		if (!ctx->playing && queued + count >= ctx->cmn.computed.min_buffer)
			audio_play(ctx);
	}
}
