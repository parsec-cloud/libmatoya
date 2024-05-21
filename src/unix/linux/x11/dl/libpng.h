// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <png.h>

#include "sym.h"


// Interface
static int (*png_sig_cmp_ptr)(png_const_bytep sig, size_t start, size_t num_to_check);
static png_structp (*png_create_read_struct_ptr)(png_const_charp user_png_ver,
	png_voidp error_ptr, png_error_ptr error_fn, png_error_ptr warn_fn);
void (*png_destroy_read_struct_ptr)(png_structpp png_ptr_ptr, png_infopp info_ptr_ptr,
	png_infopp end_info_ptr_ptr);
static png_infop (*png_create_info_struct_ptr)(png_const_structrp png_ptr);
void (*png_destroy_info_struct_ptr)(png_const_structrp png_ptr, png_infopp info_ptr_ptr);
static void (*png_set_read_fn_ptr)(png_structrp png_ptr, png_voidp io_ptr, png_rw_ptr read_data_fn);
static png_voidp (*png_get_io_ptr_ptr)(png_const_structrp png_ptr);
static void (*png_set_sig_bytes_ptr)(png_structrp png_ptr, int num_bytes);
static void (*png_read_info_ptr)(png_structrp png_ptr, png_inforp info_ptr);
static png_uint_32 (*png_get_IHDR_ptr)(png_structp png_ptr, png_infop info_ptr, png_uint_32 *width, png_uint_32 *height,
	int *bit_depth, int *color_type, int *interlace_method, int *compression_method, int *filter_method);
static void (*png_set_add_alpha_ptr)(png_structp png_ptr, png_uint_32 filler, int flags);
static int (*png_set_interlace_handling_ptr)(png_structrp png_ptr);
static void (*png_read_update_info_ptr)(png_structrp png_ptr, png_inforp info_ptr);
static void (*png_read_row_ptr)(png_structrp png_ptr, png_bytep row, png_bytep display_row);
static jmp_buf *(*png_set_longjmp_fn_ptr)(png_structrp png_ptr, png_longjmp_ptr longjmp_fn, size_t jmp_buf_size);

#define png_sig_cmp png_sig_cmp_ptr
#define png_create_read_struct png_create_read_struct_ptr
#define png_destroy_read_struct png_destroy_read_struct_ptr
#define png_create_info_struct png_create_info_struct_ptr
#define png_destroy_info_struct png_destroy_info_struct_ptr
#define png_set_read_fn png_set_read_fn_ptr
#define png_get_io_ptr png_get_io_ptr_ptr
#define png_set_sig_bytes png_set_sig_bytes_ptr
#define png_read_info png_read_info_ptr
#define png_get_IHDR png_get_IHDR_ptr
#define png_set_add_alpha png_set_add_alpha_ptr
#define png_set_interlace_handling png_set_interlace_handling_ptr
#define png_read_update_info png_read_update_info_ptr
#define png_read_row png_read_row_ptr
#define png_set_longjmp_fn png_set_longjmp_fn_ptr


// Runtime open

static MTY_Atomic32 LIBPNG_LOCK;
static MTY_SO *LIBPNG_SO;
static bool LIBPNG_INIT;

static void __attribute__((destructor)) libpng_global_destroy(void)
{
	MTY_GlobalLock(&LIBPNG_LOCK);

	MTY_SOUnload(&LIBPNG_SO);
	LIBPNG_INIT = false;

	MTY_GlobalUnlock(&LIBPNG_LOCK);
}

static bool libpng_global_init(void)
{
	MTY_GlobalLock(&LIBPNG_LOCK);

	if (!LIBPNG_INIT) {
		bool r = true;

		LIBPNG_SO = MTY_SOLoad("libpng16.so.16");
		if (!LIBPNG_SO)
			LIBPNG_SO = MTY_SOLoad("libpng16.so");

		if (!LIBPNG_SO)
			LIBPNG_SO = MTY_SOLoad("libpng.so");

		if (!LIBPNG_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBPNG_SO, png_sig_cmp);
		LOAD_SYM(LIBPNG_SO, png_create_read_struct);
		LOAD_SYM(LIBPNG_SO, png_destroy_read_struct);
		LOAD_SYM(LIBPNG_SO, png_create_info_struct);
		LOAD_SYM(LIBPNG_SO, png_destroy_info_struct);
		LOAD_SYM(LIBPNG_SO, png_set_read_fn);
		LOAD_SYM(LIBPNG_SO, png_get_io_ptr);
		LOAD_SYM(LIBPNG_SO, png_set_sig_bytes);
		LOAD_SYM(LIBPNG_SO, png_read_info);
		LOAD_SYM(LIBPNG_SO, png_get_IHDR);
		LOAD_SYM(LIBPNG_SO, png_set_add_alpha);
		LOAD_SYM(LIBPNG_SO, png_set_interlace_handling);
		LOAD_SYM(LIBPNG_SO, png_read_update_info);
		LOAD_SYM(LIBPNG_SO, png_read_row);
		LOAD_SYM(LIBPNG_SO, png_set_longjmp_fn);

		except:

		if (!r)
			libpng_global_destroy();

		LIBPNG_INIT = r;
	}

	MTY_GlobalUnlock(&LIBPNG_LOCK);

	return LIBPNG_INIT;
}
