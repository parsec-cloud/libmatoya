// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include "dl/libjpeg.h"
#include "dl/libpng.h"


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

	// Allocate core structs
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	// Set error handling -- libpng will call abort() by default!
	if (setjmp(png_jmpbuf(png_ptr)))
		return NULL;

	// Set up input buffer and read header info, offset by 8 bytes to skip past sig bytes
	struct png_io io = {
		.input = input,
		.size = size,
		.offset = 8,
	};

	png_set_read_fn(png_ptr, &io, decompress_png_io);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	int32_t color_type = 0;
	int32_t bit_depth = 0;
	png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type, NULL, NULL, NULL);

	// Transform: Set interlaced
	png_set_interlace_handling(png_ptr);

	// Transform: Add alpha channel for RGB images
	if (color_type == PNG_COLOR_TYPE_RGB)
		png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);

	// Apply transforms
	png_read_update_info(png_ptr, info_ptr);

	// Read rows into image buffer
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


struct png_out {
	uint8_t *buf;
	size_t size;
	size_t cap;
};

static void compress_png_io(png_structp png_ptr, png_bytep data, size_t size)
{
	struct png_out *out = png_get_io_ptr(png_ptr);

	if (out->size + size > out->cap) {
		out->cap = (out->size + size) * 2;
		out->buf = MTY_Realloc(out->buf, out->cap, 1);
	}

	memcpy(out->buf + out->size, data, size);
	out->size += size;
}

static void compress_png_flush(png_structp png_ptr)
{
}

static void *compress_png(const void *input, uint32_t width, uint32_t height, size_t *output_size)
{
	if (!libpng_global_init())
		return NULL;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return NULL;

	png_infop info_ptr = png_create_info_struct(png_ptr);

	// Allocated before setjmp so it survives a longjmp back here
	struct png_out *out = MTY_Alloc(1, sizeof(struct png_out));
	void *image = NULL;

	// Set error handling -- libpng will call abort() by default!
	if (setjmp(png_jmpbuf(png_ptr)))
		goto except;

	png_set_write_fn(png_ptr, out, compress_png_io, compress_png_flush);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	for (uint32_t y = 0; y < height; y++)
		png_write_row(png_ptr, (png_const_bytep) input + (size_t) y * width * 4);

	png_write_end(png_ptr, NULL);

	image = out->buf;
	*output_size = out->size;
	out->buf = NULL;

	except:

	png_destroy_write_struct(&png_ptr, &info_ptr);
	MTY_Free(out->buf);
	MTY_Free(out);

	return image;
}


// Public

void *MTY_CompressImage(MTY_ImageCompression method, const void *input, uint32_t width,
	uint32_t height, size_t *outputSize)
{
	*outputSize = 0;

	if (method == MTY_IMAGE_COMPRESSION_PNG)
		return compress_png(input, width, height, outputSize);

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
