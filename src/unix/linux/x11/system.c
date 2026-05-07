// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // readlink

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
	return get_os_release();
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
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

static uint32_t get_os_release()
{
	bool r = false;
	char *os_release = NULL;
	bool is_ubuntu = false;
	bool is_deprecated = false;
	uint32_t release = MTY_OS_LINUX;

	size_t size = 0;
	os_release = MTY_ReadFile("/etc/os-release", &size);
	if (!os_release || size == 0)
		return release;

	char *line_ptr = NULL, *line, *tok_ptr, *key, *value;

	while (line) {
		key = MTY_Strtok(line, "=", &tok_ptr);
		if (!key)
			goto next_line;

		value = MTY_Strtok(NULL, "\n", &tok_ptr);
		if (!value)
			goto next_line;

		if (!strcmp(key, "NAME") && !strcmp(value, "\"Ubuntu\"")) {
			release &= ~MTY_OS_LINUX;
			release |= MTY_OS_UBUNTU;
		} else if (!strcmp(key, "VERSION_ID") && value[0] == '"' && value[strlen(value) - 1] == '"') {
			char *val_ptr = NULL;
			char *major = MTY_Strtok(value + 1, ".", &val_ptr);
			uint8_t major_num = (uint8_t) atoi(major);
			char *minor = MTY_Strtok(NULL, "\"", &val_ptr);
			uint8_t minor_num = (uint8_t) atoi(minor);

			release |= (major_num << 8) | minor_num;
		}

		next_line:

		line = MTY_Strtok(NULL, "\n", &line_ptr);
	}

	return release;
}