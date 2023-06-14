// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#ifdef _WIN32
#include <intrin.h>
#endif

#define MTY_VECTOR_MAGIC 0xAF

struct MTY_Vector {
	uint8_t  magic; // should be 0xAF, or else this structure isn't valid
	uint8_t  log2Capacity;
	uint16_t elementSizeInBytes;
	uint32_t length; // <- super important that this is the final data member, and that it's 32 bits
};

static int integerLogTwo(int v)
{
	unsigned long result = 0;
#ifdef _WIN32
	_BitScanReverse(&result, (uint32_t)v);
#else
	if (v & 0xFFFF0000) { v >>= 16; result |= 16; }
	if (v & 0x0000FF00) { v >>=  8; result |=  8; }
	if (v & 0x000000F0) { v >>=  4; result |=  4; }
	if (v & 0x0000000C) { v >>=  2; result |=  2; }
	if (v & 0x00000002) { v >>=  1; result |=  1; }
#endif
	return result;
}

static int roundUpToPowerOfTwo(int v)
{
	return 1 << (integerLogTwo(MTY_MAX(1,v) - 1) + 1);
}

void* MTY_VectorCreate(uint32_t length, uint16_t elementSizeInBytes)
{
	const uint32_t capacity = roundUpToPowerOfTwo(length);
	MTY_Vector* ctx = (MTY_Vector*)MTY_Alloc(1, sizeof(MTY_Vector) + capacity * elementSizeInBytes);
	ctx->magic              = MTY_VECTOR_MAGIC;
	ctx->log2Capacity       = (uint8_t)integerLogTwo(capacity);
	ctx->elementSizeInBytes = (uint16_t)elementSizeInBytes;
	ctx->length             = length;
	return ctx + 1;
}

static MTY_Vector *vector_convert_pointer(void *pointer, char *file, int line)
{
	if(pointer == NULL)
		MTY_LogFatal("pointer is NULL in %s:%d", file, line);
	MTY_Vector* ctx = ((MTY_Vector*)pointer) - 1;
	if(ctx == NULL)
		MTY_LogFatal("ctx is NULL in %s:%d", ctx->magic, MTY_VECTOR_MAGIC, file, line);
	if (ctx->magic == MTY_VECTOR_MAGIC)
		return ctx;
	MTY_LogFatal("magic byte is %2x, should be %2x (probably bad pointer) in %s:%d", ctx->magic, MTY_VECTOR_MAGIC, file, line);
	return NULL;
}

void MTY_VectorDestroy(void *pointer)
{
	if(pointer == NULL)
		return;
	MTY_Vector *ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if(ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	MTY_Free(ctx);
}

bool MTY_VectorBoundsCheck(void *pointer, uint32_t index, char* file, int line)
{
	MTY_Vector *ctx = vector_convert_pointer(pointer, file, line);
	if(ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL in %s:%d", file, line);
	if(index >= ctx->length) {
		MTY_LogFatal("MTY_Vector index %d >= %d in %s:%d", index, ctx->length, file, line);
	}
	return true;
}

bool MTY_VectorSpaceCheck(void* pointer, uint32_t space, char* file, int line)
{
	MTY_Vector* ctx = vector_convert_pointer(pointer, file, line);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL in %s:%d", file, line);
	const uint32_t remaining = (1UL << ctx->log2Capacity) - ctx->length;
	if (remaining < space) {
		MTY_LogFatal("%d space needed, but only %d remaining in %s:%d.", space, remaining, file, line);
	}
	return true;
}

void *MTY_VectorReserve(void* pointer, uint32_t capacity)
{
	MTY_Vector* ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	capacity = roundUpToPowerOfTwo(capacity);
	ctx = MTY_Realloc(ctx, 1, sizeof(MTY_Vector) + capacity * ctx->elementSizeInBytes);
	ctx->log2Capacity = (uint8_t)integerLogTwo(capacity);
	return ctx + 1;
}

void *MTY_VectorResize(void* pointer, uint32_t length, uint16_t elementSizeInBytes)
{
	if(pointer == NULL)
		pointer = MTY_VectorCreate(0, elementSizeInBytes);
	MTY_Vector *ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	if(elementSizeInBytes != ctx->elementSizeInBytes)
		MTY_LogFatal("elementSizeInBytes should be %d but is %d", ctx->elementSizeInBytes, elementSizeInBytes);
	const bool tooSmall = length >  (1UL << (ctx->log2Capacity  ));
	const bool tooBig   = length <= (1UL << (ctx->log2Capacity-1));
	if(tooBig || tooSmall) 
		ctx = MTY_VectorReserve(pointer, length);
	ctx->length = length;
	return ctx + 1;	
}

void *MTY_VectorPushBack(void* pointer, uint32_t items, void* item)
{
	MTY_Vector* ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	ctx = MTY_VectorResize(pointer, ctx->length + items, ctx->elementSizeInBytes);
	memcpy((uint8_t*)(ctx + 1) + ctx->length * ctx->elementSizeInBytes, item, items * ctx->elementSizeInBytes);
	return ctx;
}

uint32_t MTY_VectorGetLength(void* pointer)
{
	MTY_Vector* ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	return ctx->length;
}

uint32_t MTY_VectorGetCapacity(void* pointer)
{
	MTY_Vector* ctx = vector_convert_pointer(pointer, __FILE__, __LINE__);
	if (ctx == NULL)
		MTY_LogFatal("vector_convert_pointer returned NULL");
	return 1UL << ctx->log2Capacity;
}
