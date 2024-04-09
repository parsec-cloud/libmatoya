// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "tlocal.h"

#include <string.h>
#include <stdio.h>

#define TLOCAL_MAX (8 * 1024)

static TLOCAL MTY_ThreadLocalState TLOCAL_STATE;
static TLOCAL uint8_t TLOCAL_BUFFER[TLOCAL_MAX];

static void tlocal_check_mem(void)
{
	if (TLOCAL_STATE.buffer)
		return;
	TLOCAL_STATE.buffer = TLOCAL_BUFFER;
	TLOCAL_STATE.size = TLOCAL_MAX;
	TLOCAL_STATE.remaining = ~0ULL;
	TLOCAL_STATE.offset = 0;
}

static void mty_tlocal_consume_bytes(size_t bytes)
{
	if (TLOCAL_STATE.remaining < bytes)
		MTY_LogFatal("Thread local storage safety error");
	TLOCAL_STATE.remaining -= bytes;
}

void *mty_tlocal(size_t size)
{
	tlocal_check_mem();
	if (size > TLOCAL_STATE.size)
		MTY_LogFatal("Thread local storage heap overflow");

	const size_t remaining = TLOCAL_STATE.size - TLOCAL_STATE.offset;
	if (size > remaining) {
		mty_tlocal_consume_bytes(remaining);
		TLOCAL_STATE.offset = 0;
	}
	mty_tlocal_consume_bytes(size);

	void *ptr = TLOCAL_STATE.buffer + TLOCAL_STATE.offset;
	memset(ptr, 0, size);

	TLOCAL_STATE.offset += size;

	return ptr;
}

char *mty_tlocal_strcpy(const char *str)
{
	tlocal_check_mem();
	size_t len = strlen(str) + 1;

	if (len > TLOCAL_STATE.size)
		len = TLOCAL_STATE.size;

	char *local = mty_tlocal(len);
	snprintf(local, len, "%s", str);

	return local;
}

MTY_ThreadLocalState MTY_ThreadLocalOverrideSafe(void *buffer, uint64_t size)
{
	const MTY_ThreadLocalState old_state = TLOCAL_STATE;
	TLOCAL_STATE = (MTY_ThreadLocalState) {.buffer = buffer, .size = size, .remaining = size};
	return old_state;
}

MTY_ThreadLocalState MTY_ThreadLocalOverrideUnsafe(void *buffer, uint64_t size)
{
	const MTY_ThreadLocalState old_state = TLOCAL_STATE;
	TLOCAL_STATE = (MTY_ThreadLocalState) {.buffer = buffer, .size = size, .remaining = ~0ULL};
	return old_state;
}

void MTY_ThreadLocalRestore(MTY_ThreadLocalState state)
{
	TLOCAL_STATE = state;
}
