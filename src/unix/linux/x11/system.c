// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>

#include "tlocal.h"

const char *MTY_GetSOExtension(void)
{
	return "so";
}

uint32_t MTY_GetPlatform(void)
{
	return MTY_OS_LINUX;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

uint32_t MTY_GetPlatformArchitecture(void)
{
	uint32_t type = 0;
	size_t size = sizeof(type);
	if (sysctlbyname("hw.cputype", &type, &size, NULL, 0) != -1) {
		switch (type) {
			case CPU_TYPE_X86:
			case CPU_TYPE_I386:
				return MTY_ARCH_X86;
			case CPU_TYPE_X86_64:
				return MTY_ARCH_X64;
			case CPU_TYPE_ARM:
				return MTY_ARCH_X86 | MTY_ARCH_ARM;
			case CPU_TYPE_ARM64:
				return MTY_ARCH_X64 | MTY_ARCH_ARM;
			case CPU_TYPE_POWERPC:
				return MTY_ARCH_X86 | MTY_ARCH_POWERPC;
			case CPU_TYPE_POWERPC64:
				return MTY_ARCH_X64 | MTY_ARCH_POWERPC;
		}
	}

	return MTY_ARCH_UNKNOWN;
}

void MTY_HandleProtocol(const char *uri, void *token)
{
	char *cmd = MTY_SprintfD("xdg-open \"%s\" 2> /dev/null &", uri);

	if (system(cmd) == -1)
		MTY_Log("'system' failed with errno %d", errno);

	MTY_Free(cmd);
}

const char *MTY_GetProcessPath(void)
{
	char tmp[MTY_PATH_MAX] = {0};

	ssize_t n = readlink("/proc/self/exe", tmp, MTY_PATH_MAX - 1);
	if (n < 0) {
		MTY_Log("'readlink' failed with errno %d", errno);
		return "/app";
	}

	return mty_tlocal_strcpy(tmp);
}

bool MTY_RestartProcess(char * const *argv)
{
	execv(MTY_GetProcessPath(), argv);
	MTY_Log("'execv' failed with errno %d", errno);

	return false;
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
