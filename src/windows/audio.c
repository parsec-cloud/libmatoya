// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <math.h>

#include <windows.h>

#include <initguid.h>
DEFINE_GUID(CLSID_MMDeviceEnumerator,  0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator,   0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient,          0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioRenderClient,    0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
DEFINE_GUID(IID_IMMNotificationClient, 0x7991EEC9, 0x7E89, 0x4D85, 0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0);

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

#define AUDIO_SAMPLE_SIZE sizeof(int16_t)
#define AUDIO_BUFFER_SIZE ((1 * 1000 * 1000 * 1000) / 100) // 1 second

#define AUDIO_REINIT_MAX_TRIES   5
#define AUDIO_REINIT_DELAY_MS  100

struct MTY_Audio {
	bool playing;
	bool notification_init;
	bool fallback;
	uint32_t sample_rate;
	uint32_t min_buffer;
	uint32_t max_buffer;
	uint8_t channels;
	WCHAR *device_id;
	UINT32 buffer_size;
	IMMDeviceEnumerator *enumerator;
	IMMNotificationClient notification;
	IAudioClient *client;
	IAudioRenderClient *render;

	bool com;
	DWORD com_thread;
};

static bool AUDIO_DEVICE_CHANGED;


// "Overridden" IMMNotificationClient

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_QueryInterface(IMMNotificationClient *This,
	REFIID riid, void **ppvObject)
{
	if (IsEqualGUID(&IID_IMMNotificationClient, riid) || IsEqualGUID(&IID_IUnknown, riid)) {
		*ppvObject = This;
		return S_OK;

	} else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}

static ULONG STDMETHODCALLTYPE _IMMNotificationClient_AddRef(IMMNotificationClient *This)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE _IMMNotificationClient_Release(IMMNotificationClient *This)
{
	return 1;
}

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_OnDeviceAdded(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_OnDeviceRemoved(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId)
{
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_OnDefaultDeviceChanged(IMMNotificationClient *This,
	EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
	if (role == eConsole)
		AUDIO_DEVICE_CHANGED = true;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE _IMMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *This,
	LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
	return S_OK;
}

static CONST_VTBL IMMNotificationClientVtbl _IMMNotificationClient = {
	.QueryInterface         = _IMMNotificationClient_QueryInterface,
	.AddRef                 = _IMMNotificationClient_AddRef,
	.Release                = _IMMNotificationClient_Release,
	.OnDeviceStateChanged   = _IMMNotificationClient_OnDeviceStateChanged,
	.OnDeviceAdded          = _IMMNotificationClient_OnDeviceAdded,
	.OnDeviceRemoved        = _IMMNotificationClient_OnDeviceRemoved,
	.OnDefaultDeviceChanged = _IMMNotificationClient_OnDefaultDeviceChanged,
	.OnPropertyValueChanged = _IMMNotificationClient_OnPropertyValueChanged,
};


// Audio

static void audio_device_destroy(MTY_Audio *ctx)
{
	if (ctx->client && ctx->playing)
		IAudioClient_Stop(ctx->client);

	if (ctx->render)
		IAudioRenderClient_Release(ctx->render);

	if (ctx->client)
		IAudioClient_Release(ctx->client);

	ctx->client = NULL;
	ctx->render = NULL;
	ctx->playing = false;
}

static HRESULT audio_get_extended_format(IMMDevice *device, WAVEFORMATEXTENSIBLE *pwfx)
{
	IPropertyStore *props = NULL;

	PROPVARIANT blob = {0};
	PropVariantInit(&blob);

	HRESULT e = IMMDevice_OpenPropertyStore(device, STGM_READ, &props);
	if (e != S_OK) {
		MTY_Log("'IMMDevice_OpenPropertyStore' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IPropertyStore_GetValue(props, &PKEY_AudioEngine_DeviceFormat, &blob);
	if (e != S_OK) {
		MTY_Log("'IPropertyStore_GetValue' failed with HRESULT 0x%X", e);
		goto except;
	}

	WAVEFORMATEXTENSIBLE *ptfx = (WAVEFORMATEXTENSIBLE *) blob.blob.pBlobData;

	if (ptfx->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		pwfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pwfx->Format.cbSize = 22;

		// Extended data
		pwfx->Samples = ptfx->Samples;
		pwfx->dwChannelMask = ptfx->dwChannelMask;
		pwfx->SubFormat = ptfx->SubFormat;
	}

	except:

	PropVariantClear(&blob);

	if (props)
		IPropertyStore_Release(props);

	return e;
}

static HRESULT audio_device_create(MTY_Audio *ctx)
{
	HRESULT e = S_OK;
	IMMDevice *device = NULL;

	if (ctx->device_id) {
		e = IMMDeviceEnumerator_GetDevice(ctx->enumerator, ctx->device_id, &device);

		if (e != S_OK && !ctx->fallback) {
			MTY_Log("'IMMDeviceEnumerator_GetDevice' failed with HRESULT 0x%X", e);
			goto except;
		}
	}

	if (!device) {
		e = IMMDeviceEnumerator_GetDefaultAudioEndpoint(ctx->enumerator, eRender, eConsole, &device);
		if (e != S_OK) {
			MTY_Log("'IMMDeviceEnumerator_GetDefaultAudioEndpoint' failed with HRESULT 0x%X", e);
			goto except;
		}
	}

	e = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, &ctx->client);
	if (e != S_OK) {
		MTY_Log("'IMMDevice_Activate' failed with HRESULT 0x%X", e);
		goto except;
	}

	WAVEFORMATEXTENSIBLE pwfx = {
		.Format.wFormatTag = WAVE_FORMAT_PCM,
		.Format.nChannels = ctx->channels,
		.Format.nSamplesPerSec = ctx->sample_rate,
		.Format.wBitsPerSample = AUDIO_SAMPLE_SIZE * 8,
		.Format.nBlockAlign = ctx->channels * AUDIO_SAMPLE_SIZE,
		.Format.nAvgBytesPerSec = ctx->sample_rate * ctx->channels * AUDIO_SAMPLE_SIZE,
	};

	// We must query extended data for greater than two channels
	if (ctx->channels > 2) {
		e = audio_get_extended_format(device, &pwfx);
		if (e != S_OK)
			goto except;
	}

	e = IAudioClient_Initialize(ctx->client, AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		AUDIO_BUFFER_SIZE, 0, &pwfx.Format, NULL);

	if (e != S_OK) {
		MTY_Log("'IAudioClient_Initialize' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IAudioClient_GetBufferSize(ctx->client, &ctx->buffer_size);
	if (e != S_OK) {
		MTY_Log("'IAudioClient_GetBufferSize' failed with HRESULT 0x%X", e);
		goto except;
	}

	e = IAudioClient_GetService(ctx->client, &IID_IAudioRenderClient, &ctx->render);
	if (e != S_OK) {
		MTY_Log("'IAudioClient_GetService' failed with HRESULT 0x%X", e);
		goto except;
	}

	except:

	if (device)
		IMMDevice_Release(device);

	if (e != S_OK)
		audio_device_destroy(ctx);

	return e;
}

MTY_Audio *MTY_AudioCreate(uint32_t sampleRate, uint32_t minBuffer, uint32_t maxBuffer, uint8_t channels,
	const char *deviceID, bool fallback)
{
	MTY_Audio *ctx = MTY_Alloc(1, sizeof(MTY_Audio));
	ctx->sample_rate = sampleRate;
	ctx->channels = channels;
	ctx->fallback = fallback;

	uint32_t frames_per_ms = lrint((float) sampleRate / 1000.0f);
	ctx->min_buffer = minBuffer * frames_per_ms;
	ctx->max_buffer = maxBuffer * frames_per_ms;

	if (deviceID)
		ctx->device_id = MTY_MultiToWideD(deviceID);

	HRESULT e = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (e != S_FALSE && e != S_OK) {
		MTY_Log("'CoInitializeEx' failed with HRESULT 0x%X", e);
		goto except;
	}

	ctx->com = true;
	ctx->com_thread = GetCurrentThreadId();

	e = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
		&IID_IMMDeviceEnumerator, &ctx->enumerator);
	if (e != S_OK) {
		MTY_Log("'CoCreateInstance' failed with HRESULT 0x%X", e);
		goto except;
	}

	ctx->notification.lpVtbl = &_IMMNotificationClient;
	e = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(ctx->enumerator, &ctx->notification);
	if (e != S_OK) {
		MTY_Log("'IMMDeviceEnumerator_RegisterEndpointNotificationCallback' failed with HRESULT 0x%X", e);
		goto except;
	}
	ctx->notification_init = true;

	e = audio_device_create(ctx);
	if (e != S_OK)
		goto except;

	except:

	if (e != S_OK)
		MTY_AudioDestroy(&ctx);

	return ctx;
}

void MTY_AudioDestroy(MTY_Audio **audio)
{
	if (!audio || !*audio)
		return;

	MTY_Audio *ctx = *audio;

	audio_device_destroy(ctx);

	if (ctx->notification_init)
		IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(ctx->enumerator, &ctx->notification);

	if (ctx->enumerator)
		IMMDeviceEnumerator_Release(ctx->enumerator);

	if (ctx->com) {
		if (GetCurrentThreadId() == ctx->com_thread) {
			CoUninitialize();

		} else {
			MTY_Log("MTY_Audio context should not be destroyed on a "
				"different thread from where it was created");
		}
	}

	MTY_Free(ctx->device_id);

	MTY_Free(ctx);
	*audio = NULL;
}

static HRESULT audio_get_queued_frames(MTY_Audio *ctx, uint32_t *out)
{
	*out = ctx->buffer_size; // indicates that the buffer is full, which also serves
	                         // as a useful return value in the case of an error.

	if (!ctx->client)
		return AUDCLNT_E_NOT_INITIALIZED;

	UINT32 padding = 0;
	HRESULT e = IAudioClient_GetCurrentPadding(ctx->client, &padding);
	if (e == S_OK)
		*out = padding;

	return e;
}

static void audio_play(MTY_Audio *ctx)
{
	if (!ctx->client)
		return;

	if (!ctx->playing) {
		HRESULT e = IAudioClient_Start(ctx->client);
		if (e != S_OK) {
			MTY_Log("'IAudioClient_Start' failed with HRESULT 0x%X", e);
			return;
		}

		ctx->playing = true;
	}
}

void MTY_AudioReset(MTY_Audio *ctx)
{
	if (!ctx->client)
		return;

	if (ctx->playing) {
		HRESULT e = IAudioClient_Stop(ctx->client);
		if (e != S_FALSE && e != S_OK) {
			MTY_Log("'IAudioClient_Stop' failed with HRESULT 0x%X", e);
			return;
		}

		e = IAudioClient_Reset(ctx->client);
		if (e != S_FALSE && e != S_OK) {
			MTY_Log("'IAudioClient_Reset' failed with HRESULT 0x%X", e);
			return;
		}

		ctx->playing = false;
	}
}

static HRESULT audio_reinit_device(MTY_Audio *ctx)
{
	audio_device_destroy(ctx);

	// When the default device is in the process of changing, this may fail so we try multiple times
	HRESULT e = AUDCLNT_E_NOT_INITIALIZED;
	for (uint8_t x = 0; x < AUDIO_REINIT_MAX_TRIES; x++) {
		e = audio_device_create(ctx);

		if (e == S_OK && ctx->client)
			break;

		MTY_Sleep(AUDIO_REINIT_DELAY_MS);
	}

	return e;
}

static bool audio_handle_device_change(MTY_Audio *ctx)
{
	if (AUDIO_DEVICE_CHANGED) {
		if (audio_reinit_device(ctx) != S_OK)
			return false;

		AUDIO_DEVICE_CHANGED = false;
	}

	return true;
}

uint32_t MTY_AudioGetQueued(MTY_Audio *ctx)
{
	uint32_t queued = 0;
	audio_get_queued_frames(ctx, &queued);
	return lrint((float) queued / ((float) ctx->sample_rate / 1000.0f));
}

void MTY_AudioQueue(MTY_Audio *ctx, const int16_t *frames, uint32_t count)
{
	if (!audio_handle_device_change(ctx))
		return;

	uint32_t queued = 0;
	HRESULT e = audio_get_queued_frames(ctx, &queued);
	if (e != S_OK) {
		MTY_Log("\"audio_get_queued_frames\" returned 0x%X. Re-initializing audio device", e);

		e = audio_reinit_device(ctx);
		if (e != S_OK) {
			MTY_Log("\"audio_reinit_device\" returned 0x%X, failed to re-initialize audio device", e);
			return;
		}

		audio_get_queued_frames(ctx, &queued);
	}

	// Stop playing and flush if we've exceeded the maximum buffer or underrun
	if (ctx->playing && (queued > ctx->max_buffer || queued == 0))
		MTY_AudioReset(ctx);

	if (ctx->buffer_size - queued >= count) {
		BYTE *buffer = NULL;
		e = IAudioRenderClient_GetBuffer(ctx->render, count, &buffer);

		if (e == S_OK) {
			memcpy(buffer, frames, count * ctx->channels * AUDIO_SAMPLE_SIZE);
			IAudioRenderClient_ReleaseBuffer(ctx->render, count, 0);
		}

		// Begin playing again when the minimum buffer has been reached
		if (!ctx->playing && queued + count >= ctx->min_buffer)
			audio_play(ctx);
	}
}
