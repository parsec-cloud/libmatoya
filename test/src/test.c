// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _CRT_SECURE_NO_WARNINGS

#include "matoya.h"

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <wchar.h>

// Framework
#include "test/test.h"

/// Modules
#include "test/memory.h"
#include "test/json.h"
#include "test/version.h"
#include "test/time.h"
#include "test/log.h"
#include "test/file.h"
#include "test/struct.h"
#include "test/system.h"
#include "test/thread.h"
#include "test/crypto.h"
#include "test/net.h"

uint64_t test_seed = 0xDEADBEEFCAFEBABEUL;

static void main_log(const char *msg, void *opaque)
{
	printf("%s\n", msg);
}

int32_t main(int32_t argc, char **argv)
{
	setlocale(LC_ALL, "en_US.UTF-8");

	MTY_SetLogFunc(main_log, NULL);

	test_seed ^= MTY_GetTime();
	const char *seed = getenv("MATOYA_TEST_SEED");
	if (seed)
		sscanf(seed, "0x%" PRIx64, &test_seed);
	printf("::notice file=%s,line=%d::MATOYA_TEST_SEED=0x%" PRIx64 "\n", __FILE__, __LINE__, test_seed);

	if (!json_main())
		return 1;

  	if (!struct_main())
		return 1;

	if (!system_main())
		return 1;

	if (!version_main())
		return 1;

	if (!time_main())
		return 1;

	if (!file_main())
		return 1;

	if (!memory_main())
		return 1;

	if (!log_main())
		return 1;

	MTY_SetLogFunc(main_log, NULL);

	if (!crypto_main())
		return 1;

	if (!thread_main())
		return 1;

	if (!net_main())
		return 1;

	return 0;
}
