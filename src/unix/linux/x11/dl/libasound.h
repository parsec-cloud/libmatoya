// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <alsa/asoundlib.h>

#include <alloca.h>
#include <errno.h>

#include "sym.h"


// Interface

static int (*snd_pcm_open_ptr)(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
static int (*snd_pcm_hw_params_any_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
static int (*snd_pcm_hw_params_set_access_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
static int (*snd_pcm_hw_params_set_format_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
static int (*snd_pcm_hw_params_set_channels_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
static int (*snd_pcm_hw_params_set_rate_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir);
static int (*snd_pcm_hw_params_ptr)(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
static int (*snd_pcm_prepare_ptr)(snd_pcm_t *pcm);
static snd_pcm_sframes_t (*snd_pcm_writei_ptr)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
static int (*snd_pcm_close_ptr)(snd_pcm_t *pcm);
static int (*snd_pcm_nonblock_ptr)(snd_pcm_t *pcm, int nonblock);
static int (*snd_pcm_status_ptr)(snd_pcm_t *pcm, snd_pcm_status_t *status);
static size_t (*snd_pcm_status_sizeof_ptr)(void);
static size_t (*snd_pcm_hw_params_sizeof_ptr)(void);
static snd_pcm_uframes_t (*snd_pcm_status_get_avail_ptr)(const snd_pcm_status_t *obj);
static snd_pcm_uframes_t (*snd_pcm_status_get_avail_max_ptr)(const snd_pcm_status_t *obj);

#define snd_pcm_open snd_pcm_open_ptr
#define snd_pcm_hw_params_any snd_pcm_hw_params_any_ptr
#define snd_pcm_hw_params_set_access snd_pcm_hw_params_set_access_ptr
#define snd_pcm_hw_params_set_format snd_pcm_hw_params_set_format_ptr
#define snd_pcm_hw_params_set_channels snd_pcm_hw_params_set_channels_ptr
#define snd_pcm_hw_params_set_rate snd_pcm_hw_params_set_rate_ptr
#define snd_pcm_hw_params snd_pcm_hw_params_ptr
#define snd_pcm_prepare snd_pcm_prepare_ptr
#define snd_pcm_writei snd_pcm_writei_ptr
#define snd_pcm_close snd_pcm_close_ptr
#define snd_pcm_nonblock snd_pcm_nonblock_ptr
#define snd_pcm_status snd_pcm_status_ptr
#define snd_pcm_status_sizeof snd_pcm_status_sizeof_ptr
#define snd_pcm_hw_params_sizeof snd_pcm_hw_params_sizeof_ptr
#define snd_pcm_status_get_avail snd_pcm_status_get_avail_ptr
#define snd_pcm_status_get_avail_max snd_pcm_status_get_avail_max_ptr


// Runtime open

static MTY_Atomic32 LIBASOUND_LOCK;
static MTY_SO *LIBASOUND_SO;
static bool LIBASOUND_INIT;

static void __attribute__((destructor)) libasound_global_destroy(void)
{
	MTY_GlobalLock(&LIBASOUND_LOCK);

	MTY_SOUnload(&LIBASOUND_SO);
	LIBASOUND_INIT = false;

	MTY_GlobalUnlock(&LIBASOUND_LOCK);
}

static bool libasound_global_init(void)
{
	MTY_GlobalLock(&LIBASOUND_LOCK);

	if (!LIBASOUND_INIT) {
		bool r = true;

		LIBASOUND_SO = MTY_SOLoad("libasound.so.2");

		if (!LIBASOUND_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBASOUND_SO, snd_pcm_open);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_any);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_access);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_format);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_channels);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_set_rate);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_prepare);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_writei);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_close);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_nonblock);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_sizeof);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_hw_params_sizeof);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_get_avail);
		LOAD_SYM(LIBASOUND_SO, snd_pcm_status_get_avail_max);

		except:

		if (!r)
			libasound_global_destroy();

		LIBASOUND_INIT = r;
	}

	MTY_GlobalUnlock(&LIBASOUND_LOCK);

	return LIBASOUND_INIT;
}
