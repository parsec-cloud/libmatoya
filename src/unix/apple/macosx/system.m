// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <AppKit/AppKit.h>
#include <mach-o/dyld.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/machine.h>

#include "tlocal.h"

const char *MTY_GetSOExtension(void)
{
	return "dylib";
}

uint32_t MTY_GetPlatform(void)
{
	uint32_t v = MTY_OS_MACOS;

	NSProcessInfo *pInfo = [NSProcessInfo processInfo];
	NSOperatingSystemVersion version = [pInfo operatingSystemVersion];

	v |= (uint32_t) version.majorVersion << 8;
	v |= (uint32_t) version.minorVersion;

	return v;
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
	NSString *nsuri = [NSString stringWithUTF8String:uri];

	if (strstr(uri, "http") == uri) {
		[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:nsuri]];

	} else {
		[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:nsuri]];
	}
}

const char *MTY_GetProcessPath(void)
{
	char tmp[MTY_PATH_MAX] = {0};
	uint32_t bsize = MTY_PATH_MAX - 1;

	int32_t e = _NSGetExecutablePath(tmp, &bsize);
	if (e != 0) {
		MTY_Log("'_NSGetExecutablePath' failed with error %d", e);
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
