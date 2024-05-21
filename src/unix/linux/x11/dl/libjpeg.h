// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <jpeglib.h>

#include "sym.h"


// Interface
static struct jpeg_error_mgr *(*jpeg_std_error_ptr)(struct jpeg_error_mgr *err);
static void (*jpeg_CreateDecompress_ptr)(j_decompress_ptr cinfo, int version, size_t structsize);
static void (*jpeg_mem_src_ptr)(j_decompress_ptr cinfo, void *buffer, long nbytes);
static int (*jpeg_read_header_ptr)(j_decompress_ptr cinfo, boolean require_image);
static boolean (*jpeg_start_decompress_ptr)(j_decompress_ptr cinfo);
static JDIMENSION (*jpeg_read_scanlines_ptr)(j_decompress_ptr cinfo, JSAMPARRAY scanlines, JDIMENSION max_lines);
static boolean (*jpeg_finish_decompress_ptr)(j_decompress_ptr cinfo);
static void (*jpeg_destroy_decompress_ptr)(j_decompress_ptr cinfo);

#define jpeg_std_error jpeg_std_error_ptr
#define jpeg_CreateDecompress jpeg_CreateDecompress_ptr
#define jpeg_mem_src jpeg_mem_src_ptr
#define jpeg_read_header jpeg_read_header_ptr
#define jpeg_start_decompress jpeg_start_decompress_ptr
#define jpeg_read_scanlines jpeg_read_scanlines_ptr
#define jpeg_finish_decompress jpeg_finish_decompress_ptr
#define jpeg_destroy_decompress jpeg_destroy_decompress_ptr


// Runtime open

static MTY_Atomic32 LIBJPEG_LOCK;
static MTY_SO *LIBJPEG_SO;
static bool LIBJPEG_INIT;

static void __attribute__((destructor)) libjpeg_global_destroy(void)
{
	MTY_GlobalLock(&LIBJPEG_LOCK);

	MTY_SOUnload(&LIBJPEG_SO);
	LIBJPEG_INIT = false;

	MTY_GlobalUnlock(&LIBJPEG_LOCK);
}

static bool libjpeg_global_init(void)
{
	MTY_GlobalLock(&LIBJPEG_LOCK);

	if (!LIBJPEG_INIT) {
		bool r = true;

		LIBJPEG_SO = MTY_SOLoad("libjpeg.so.8");
		if (!LIBJPEG_SO)
			LIBJPEG_SO = MTY_SOLoad("libjpeg.so.62");

		if (!LIBJPEG_SO)
			LIBJPEG_SO = MTY_SOLoad("libjpeg.so");

		if (!LIBJPEG_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBJPEG_SO, jpeg_std_error);
		LOAD_SYM(LIBJPEG_SO, jpeg_CreateDecompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_mem_src);
		LOAD_SYM(LIBJPEG_SO, jpeg_read_header);
		LOAD_SYM(LIBJPEG_SO, jpeg_start_decompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_read_scanlines);
		LOAD_SYM(LIBJPEG_SO, jpeg_finish_decompress);
		LOAD_SYM(LIBJPEG_SO, jpeg_destroy_decompress);

		except:

		if (!r)
			libjpeg_global_destroy();

		LIBJPEG_INIT = r;
	}

	MTY_GlobalUnlock(&LIBJPEG_LOCK);

	return LIBJPEG_INIT;
}
