/** logger.h
 *
 * Logging-related stuff.
 */

#ifndef _MY_LOGGER_H_
#define _MY_LOGGER_H_

#include <stdarg.h>

typedef struct logger Logger;

Logger *logger_new(const char *logname);
void logger_free(Logger *logger);

void llog(Logger *logger, const char *fmt, ...);
void vllog(Logger *logger, const char *fmt, va_list args);

// Log to the log passed by logger_init.
void slog(const char *fmt, ...);

// Init the ba system log.
void logger_init(const char *logpath);
void logger_rollover();
void logger_shutdown();

#endif /* _API_INC_H_ */
