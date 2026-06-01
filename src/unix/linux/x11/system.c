// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // readlink

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

MTY_OSInfo MTY_GetPlatformOSInfo(void)
{
	MTY_OSInfo info = {
		.os = MTY_OS_LINUX,
		.name = MTY_GetPlatformString(),
		.valid_mask.name = true,
	};
	char *os_release = NULL;

	size_t size = 0;
	os_release = MTY_ReadFile("/etc/os-release", &size);
	if (!os_release || size == 0)
		return info;
	
	bool seen_pretty_name = false; // "PRETTY_NAME" is preferred over "NAME"
	char *line_ptr = NULL, *line, *tok_ptr, *key, *value;
	
	line = MTY_Strtok(os_release, "\n", &line_ptr);
	while (line) {
		key = MTY_Strtok(line, "=", &tok_ptr);
		if (!key)
			goto next_line;

		// We don't worry about quoted strings, a copy them verbatim, except in version numbers.
		value = MTY_Strtok(NULL, "\n", &tok_ptr);
		if (!value)
			goto next_line;

		if (!strcmp(key, "NAME") && !seen_pretty_name) {
			info.name = mty_tlocal_strcpy(value);
			info.valid_mask.name_pretty = true;
		} else if (!strcmp(key, "PRETTY_NAME")) {
			info.name = mty_tlocal_strcpy(value);
			info.valid_mask.name_pretty = true;
			seen_pretty_name = true;
		} else if (!strcmp(key, "VERSION")) {
			info.version_pretty = mty_tlocal_strcpy(value);
			info.valid_mask.version_pretty = true;
		} else if (!strcmp(key, "ID")) {
			info.id = mty_tlocal_strcpy(value);
			info.valid_mask.id = true;
		} else if (!strcmp(key, "ID_LIKE")) {
			info.base_id = mty_tlocal_strcpy(value);
			info.valid_mask.base_id = true;
		} else if (!strcmp(key, "RELEASE_TYPE")) {
			static char const* release_type_name[] = {
				[MTY_OS_RELEASE_STABLE] = "stable",
				[MTY_OS_RELEASE_LTS] = "stable",
				[MTY_OS_RELEASE_DEVELOPMENT] = "development",
				[MTY_OS_RELEASE_EXPERIMENT] = "experiment",
			};
			for (int i = MTY_OS_RELEASE_STABLE; i < (sizeof(release_type_name)/sizeof(release_type_name[0])); ++i) {
				if (!strcmp(value, release_type_name[i])) {
					info.release_type = (MTY_OSReleaseType)i;
					break;
				}
			}
			info.valid_mask.release_type = true;
		} else if (!strcmp(key, "VERSION_ID")) {
			// check for quoted string, skip ahead if need be
			if (value[0] == '"')
				value++;
			char* val_ptr = NULL;
			char* digit_str = MTY_Strtok(value, "._-\"\n", &val_ptr);
			for (int i = 0; i < (sizeof(info.version.digits) / sizeof(info.version.digits[0])); ++i) {
				if (!digit_str) break;
				info.version.digits[i] = atoi(digit_str);
				digit_str = MTY_Strtok(NULL, "._-\"\n", &val_ptr);
			}
			info.valid_mask.version = true;
		}

		next_line:

		line = MTY_Strtok(NULL, "\n", &line_ptr);
	}

	MTY_Free(os_release);

	return info;
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

bool MTY_StartInProcess(const char *path, char * const *argv, const char *dir)
{
	if (dir) {
		if (chdir(dir) == -1) {
			MTY_Log("'chdir' failed with errno %d", errno);
			return false;
		}
	}

	if (!path)
		path = MTY_GetProcessPath();

	execv(path, argv);
	MTY_Log("'execv' failed with errno %d", errno);

	return false;
}

bool MTY_RestartProcess(char * const *argv)
{
	return MTY_StartInProcess(NULL, argv, NULL);
}

void *MTY_GetJNIEnv(void)
{
	return NULL;
}
