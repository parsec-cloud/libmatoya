// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "dl/libjpeg.h"
#include "dl/libpng.h"
//#include <png.h>


// JPEG

struct jpeg_error {
	struct jpeg_error_mgr mgr;
	jmp_buf jmp;
};

static void decompress_jpeg_error(j_common_ptr cinfo)
{
	struct jpeg_error *err = (struct jpeg_error *) cinfo->err;
	(*(cinfo->err->output_message))(cinfo);

	longjmp(err->jmp, 1);
}

static void *decompress_jpeg(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	if (!libjpeg_global_init())
		return NULL;

	struct jpeg_decompress_struct cinfo = {
		.out_color_space = JCS_RGB,
	};

	// Error manager -- libjpeg default behavior calls exit()!
	struct jpeg_error jerr = {0};
	cinfo.err = jpeg_std_error(&jerr.mgr);
	jerr.mgr.error_exit = decompress_jpeg_error;

	if (setjmp(jerr.jmp))
		return NULL;

	bool r = true;
	uint8_t *image = NULL;

	// Create the decompressor
	jpeg_CreateDecompress(&cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));

	// Set up input buffer and read JPEG header info
	jpeg_mem_src(&cinfo, (void *) input, size);

	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		r = false;
		goto except;
	}

	// Decompress
	jpeg_start_decompress(&cinfo);

	// Copy RGB data to RGBA
	*width = cinfo.output_width;
	*height = cinfo.output_height;

	image = MTY_Alloc(*width * *height * 4, 1);

	for (uint32_t y = 0; y < *height; y++) {
		uint8_t *row = image + *width * y * 4;
		jpeg_read_scanlines(&cinfo, &row, 1);

		for (int32_t x = *width - 1; x >= 0; x--) {
			uint32_t dst = x * 4;
			uint32_t src = x * 3;

			row[dst + 3] = 0xFF;
			row[dst + 2] = row[src + 2];
			row[dst + 1] = row[src + 1];
			row[dst] = row[src];
		}
	}

	// Cleanup
	jpeg_finish_decompress(&cinfo);

	except:

	if (!r) {
		MTY_Free(image);
		image = NULL;
	}

	jpeg_destroy_decompress(&cinfo);

	return image;
}


// PNG

struct png_io {
	const void *input;
	size_t offset;
	size_t size;
};

static void decompress_png_io(png_structp png_ptr, png_bytep buf, size_t size)
{
	struct png_io *io = png_get_io_ptr(png_ptr);

	if (size + io->offset < io->size) {
		memcpy(buf, io->input + io->offset, size);
		io->offset += size;

	} else {
		memset(buf, 0, size);
	}
}

static void *decompress_png(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	if (!libpng_global_init())
		return NULL;

	if (png_sig_cmp(input, 0, 8))
		return NULL;

	png_byte bit_depth = 0;
	png_byte color_type = 0;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (setjmp(png_jmpbuf(png_ptr)))
		return NULL;

	struct png_io io = {
		.input = input,
		.size = size,
		.offset = 8,
	};

	png_set_read_fn(png_ptr, &io, decompress_png_io);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	
	/* Read any color_type into 8bit depth, RGBA format. */
	/* See http://www.libpng.org/pub/png/libpng-manual.txt */
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	/* PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth. */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	/* These color_type don't have an alpha channel then fill it with 0xff. */
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);
	uint8_t *image = MTY_Alloc(*width * *height * 4, 1);

	for (uint32_t y = 0; y < *height; y++) {
		uint8_t *row = image + *width * y * 4;
		png_read_row(png_ptr, row, NULL);
	}

	// Cleanup
	png_destroy_info_struct(png_ptr, &info_ptr);
	png_destroy_read_struct(&png_ptr, NULL, NULL);

	return image;
}


// Public

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	*outputSize = 0;

	return NULL;
}

void *MTY_DecompressImage(const void *input, size_t size, uint32_t *width, uint32_t *height)
{
	const uint8_t *input8 = input;

	// JPEG
	if (input8[0] == 0xFF && input8[1] == 0xD8 && input8[2] == 0xFF) {
		return decompress_jpeg(input, size, width, height);

	// PNG
	} else if (input8[0] == 0x89 && input8[1] == 0x50 && input8[2] == 0x4E) {
		return decompress_png(input, size, width, height);
	}

	return NULL;
}

void *MTY_GetProgramIcon(const char *path, uint32_t *width, uint32_t *height)
{
	return NULL;
}
