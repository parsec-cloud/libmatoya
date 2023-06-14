// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "tlocal.h"

#include <string.h>
#include <stdio.h>

#define TLOCAL_MAX (8 * 1024)
#define TLOCAL_SCOPES 8

static TLOCAL uint8_t  TLOCAL_HEAP[TLOCAL_MAX];
static TLOCAL uint8_t *TLOCAL_BASE[TLOCAL_SCOPES];
static TLOCAL size_t   TLOCAL_OFFSET[TLOCAL_SCOPES];
static TLOCAL uint16_t TLOCAL_SIZE[TLOCAL_SCOPES] = {TLOCAL_MAX};
static TLOCAL uint16_t TLOCAL_SCOPE;

void MTY_TempMemoryBegin(uint8_t *pointer, uint16_t size)
{	
	TLOCAL_SCOPE++;
	if (TLOCAL_SCOPE == TLOCAL_SCOPES)
		MTY_LogFatal("Thread local storage tried to push too many scopes");
	TLOCAL_BASE[TLOCAL_SCOPE] = pointer;
	TLOCAL_SIZE[TLOCAL_SCOPE] = size;
}

void MTY_TempMemoryEnd()
{
	if (TLOCAL_SCOPE == 0)
		MTY_LogFatal("Thread local storage tried to pop too many scopes");
	--TLOCAL_SCOPE;
}

void *mty_tlocal(size_t size)
{
	if (size > TLOCAL_SIZE[TLOCAL_SCOPE])
		MTY_LogFatal("Thread local storage heap overflow");

	if (TLOCAL_OFFSET[TLOCAL_SCOPE] + size > TLOCAL_SIZE[TLOCAL_SCOPE])	{
		if(TLOCAL_SCOPE != 0)
			MTY_LogFatal("Thread local storage heap overflow");
		TLOCAL_OFFSET[TLOCAL_SCOPE] = 0; // unsafe!
	}

	TLOCAL_BASE[0] = TLOCAL_HEAP;

	void *ptr = TLOCAL_BASE[TLOCAL_SCOPE] + TLOCAL_OFFSET[TLOCAL_SCOPE];
	memset(ptr, 0, size);

	TLOCAL_OFFSET[TLOCAL_SCOPE] += size;

	return ptr;
}

char *mty_tlocal_strcpy(const char *str)
{
	size_t len = strlen(str) + 1;

	if (len > TLOCAL_SIZE[TLOCAL_SCOPE])
		len = TLOCAL_SIZE[TLOCAL_SCOPE];

	char *local = mty_tlocal(len);
	snprintf(local, len, "%s", str);

	return local;
}
