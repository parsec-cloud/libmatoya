//
// Minimal per-process debug logger:
// - Include ptrace.h in your .c file.
// - Use ptrace() like printf() in your code.
// - Trace calls generate code only in debug builds.  They effectively
//   don't exist in release builds (the compiler's optimizer removes the
//   inserted dummy statements in release mode).
// - Trace output is written to a log file in the current user's temporary
//   directory (same as %TEMP% environment variable on Windows).
// - The log filename is different for each process ID, for example
//   "PID_12345.log", so if multiple processes are logging at the same
//   time, the logs don't collide. 
// - The log files are not kept open between writes.  The log is opened
//   before each write and closed after each write.  This is slower than
//   keeping the file open all the time, but more reliable at not losing
//   the end of the log if the app should happen to terminate abruptly
//   (since fflush isn't always reliable).
//
// Limitations:
// - Only supported on Windows.  These calls do nothing on other platforms.
// - The size of each formatted text string must not exceed 4K bytes. 
//

#pragma once

#if (defined(DEBUG) || defined(_DEBUG) || defined(CC_DEBUG))
# define ptrace plog_printf
#else
  // Generate a dummy expression in release builds, which gets optimized
  // out by the compiler.  We only want the trace code in debug builds.
# define ptrace 1 ? (void)0 : plog_printf
#endif

#ifdef __WINDOWS__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

//
// Writes a line of formatted text to the log file for the current process.
// The format string uses the same format as printf in the C runtime library.
//
// NOTE:  ptrace() calls map to this function in debug builds.
//
static void plog_printf(const char *format, ...)
{
	static bool busy = false;

	if (busy)
		return;
	busy = true;

	// Generate the formatted text into a buffer.
	static char buffer[4096] = {0};
	va_list vv;
	va_start(vv, format);
	vsprintf_s(buffer, sizeof(buffer), format, vv);
	va_end(vv);

	// Generate the log file/pathname for the current process.
	// The format is "[Windows temporary directory]\PID_xxxx.log",
	// where "xxxx" is the process ID of the calling program.
	char filename[MAX_PATH] = {0};
	uint32_t process_id = GetCurrentProcessId();
	sprintf_s(filename, sizeof(filename), "PID_%u.log", (unsigned)process_id);
	char pathname[MAX_PATH] = {0};
	GetTempPathA((DWORD)sizeof(pathname), pathname);
	strcat_s(pathname, sizeof(pathname), filename);

	// Append the formatted text string to the end of the log file.
	// Note if this fails there is no error.  The log is simply not written.
	FILE *fp = NULL;
	if (fopen_s(&fp, pathname, "a") || fp == NULL) {
		busy = false;
		return;	// Failed opening log file!
	}
	fprintf(fp, "%.3f:  ", GetTickCount64() / 1000.);
	fprintf(fp, "%s\n", buffer);
	fclose(fp);

//
// NOTE:
// Change the conditional below to #if 0 or #if 1 depending on whether
// you want the trace output to *also* go to stdout.
//
#if 1

	printf("PID_%u:%.3f  %s\n", (unsigned)process_id, GetTickCount64() / 1000., buffer);
	fflush(stdout);

#endif

	busy = false;
}

#else // !__WINDOWS__

static void plog_printf(const char *format, ...)
{
	(void)format;

	// This currently does nothing on non-Windows platforms.
}

#endif
