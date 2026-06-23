#ifndef DEBUGLOG_H_
#define DEBUGLOG_H_

#ifdef __cplusplus
extern "C" {
#endif

void debuglog_init(const char *argv0);
void debuglog_close(void);
void debuglog_printf(const char *fmt, ...);
void debuglog_verbose_printf(const char *fmt, ...);
int debuglog_is_verbose(void);

#ifdef __cplusplus
}
#endif

#endif
