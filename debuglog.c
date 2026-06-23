#include "debuglog.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(WINDOWS)
#include <windows.h>
#else
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#define LOG_MAX_BYTES (2 * 1024 * 1024)

static FILE *log_file;
static int verbose_enabled;
static char log_path[1024];
static char debug_flag_path[1024];

static void dirname_from_argv0(const char *argv0, char *out, size_t outlen)
{
	const char *slash;
	const char *backslash;
	const char *end;

	if (outlen == 0) return;
	out[0] = 0;
	if (argv0 == NULL || argv0[0] == 0) {
		strncpy(out, ".", outlen - 1);
		out[outlen - 1] = 0;
		return;
	}
	slash = strrchr(argv0, '/');
	backslash = strrchr(argv0, '\\');
	end = slash > backslash ? slash : backslash;
	if (end == NULL) {
		strncpy(out, ".", outlen - 1);
		out[outlen - 1] = 0;
		return;
	}
	if ((size_t)(end - argv0) >= outlen) end = argv0 + outlen - 1;
	memcpy(out, argv0, (size_t)(end - argv0));
	out[end - argv0] = 0;
}

static void path_join(char *out, size_t outlen, const char *dir, const char *name)
{
	const char *sep = "/";
	size_t len;

	if (outlen == 0) return;
	len = strlen(dir);
	if (len > 0 && (dir[len - 1] == '/' || dir[len - 1] == '\\')) sep = "";
	snprintf(out, outlen, "%s%s%s", dir, sep, name);
}

static long file_size(const char *path)
{
#if defined(WINDOWS)
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data)) return -1;
	return (long)data.nFileSizeLow;
#else
	struct stat st;
	if (stat(path, &st) != 0) return -1;
	return (long)st.st_size;
#endif
}

static int file_exists(const char *path)
{
#if defined(WINDOWS)
	DWORD attr = GetFileAttributesA(path);
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
	return access(path, F_OK) == 0;
#endif
}

static void rotate_log_if_needed(void)
{
	char backup[1100];

	if (file_size(log_path) < LOG_MAX_BYTES) return;
	snprintf(backup, sizeof(backup), "%s.1", log_path);
	remove(backup);
	rename(log_path, backup);
}

static void timestamp(char *out, size_t outlen)
{
#if defined(WINDOWS)
	SYSTEMTIME st;
	GetLocalTime(&st);
	snprintf(out, outlen, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
		(unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
		(unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
		(unsigned)st.wMilliseconds);
#else
	time_t now = time(NULL);
	struct tm tm_now;
	localtime_r(&now, &tm_now);
	snprintf(out, outlen, "%04d-%02d-%02d %02d:%02d:%02d.000",
		tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
		tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
#endif
}

static void vlog_printf(const char *fmt, va_list ap)
{
	char ts[32];

	if (log_file == NULL) return;
	timestamp(ts, sizeof(ts));
	fprintf(log_file, "%s ", ts);
	vfprintf(log_file, fmt, ap);
	fputc('\n', log_file);
	fflush(log_file);
}

void debuglog_init(const char *argv0)
{
	char dir[1024];

	dirname_from_argv0(argv0, dir, sizeof(dir));
	path_join(log_path, sizeof(log_path), dir, "MotionCal.log");
	path_join(debug_flag_path, sizeof(debug_flag_path), dir, "MotionCal.debug");
	rotate_log_if_needed();
	log_file = fopen(log_path, "a");
	verbose_enabled = file_exists(debug_flag_path);
	debuglog_printf("MotionCal start log=%s verbose=%s", log_path,
		verbose_enabled ? "on" : "off");
}

void debuglog_close(void)
{
	debuglog_printf("MotionCal exit");
	if (log_file != NULL) {
		fclose(log_file);
		log_file = NULL;
	}
}

void debuglog_printf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vlog_printf(fmt, ap);
	va_end(ap);
}

void debuglog_verbose_printf(const char *fmt, ...)
{
	va_list ap;
	if (!verbose_enabled) return;
	va_start(ap, fmt);
	vlog_printf(fmt, ap);
	va_end(ap);
}

int debuglog_is_verbose(void)
{
	return verbose_enabled;
}
