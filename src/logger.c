/* logger.c
 *
 * Rotating log files.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "logger.h"

struct logger {
  FILE *logout;

  // The logname path and prefix: "users/foo/log/worldname"
  // This has "-YYYY-MM-DD.log" appended to it, and rotated daily.
  char *logname;

  // YYYY-MM-DD
  char *datestr;

  // Date string.
  struct tm logday;

  // So we don't have overlapping logs.
  pthread_mutex_t logmutex;

  // TODO: Allowing users to rotate logs based on their timezone,
  //       not forcing all to use central time.
};

pthread_mutexattr_t pth_recurse;
static Logger *syslogger;

static int
mkdir_p(const char *pathname) {
  char pbuff[200];
  char *p;
  if (mkdir(pathname, 0755)) {
    if (errno == EEXIST) { return 0; }
    if (errno == ENOENT) {
      snprintf(pbuff, 200, "%s", pathname);
      p = strrchr(pbuff, '/');
      if (p) {
        *(p) = '\0';
        mkdir_p(pbuff);
        return mkdir(pathname, 0777);
      }
    }
  }
  return 0;
}

Logger *
logger_new(const char *logname) {
  time_t now;
  char timebuff[20];
  char fpath[200], *ptr;
  Logger *logger = malloc(sizeof(Logger));

  logger->logname = strdup(logname);
  // Create the directory if it doesn't exist.
  snprintf(fpath, 200, "%s", logname);
  ptr = strrchr(fpath, '/');
  if (ptr) {
    *(ptr) = '\0';
    mkdir_p(fpath);
  }

  now = time(NULL);
  localtime_r(&now, &logger->logday);

  strftime(timebuff, 20, "%F", &logger->logday);
  logger->datestr = strdup(timebuff);

  pthread_mutex_init(&logger->logmutex, &pth_recurse);
  logger->logout = NULL;

  llog(logger, "-- LOG STARTED --");

  return logger;
}

void
logger_free(Logger *logger) {
  llog(logger, "-- LOG ENDED --");

  if (logger->logout) {
    fclose(logger->logout);
  }

  free(logger->logname);
  free(logger->datestr);
  pthread_mutex_destroy(&logger->logmutex);
}

void
llog(Logger *logger, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vllog(logger, fmt, args);
  va_end(args);
}

void
vllog(Logger *logger, const char *fmt, va_list args) {
  struct tm today;
  time_t now;
  char filename[200];
  char timebuff[20];
  int  logrollover = 0;

  // Lock up.
  pthread_mutex_lock(&logger->logmutex);

  now = time(NULL);
  localtime_r(&now, &today);

  if (today.tm_yday != logger->logday.tm_yday) {
    // Date has advanced.
    if (logger->logout) {
      fprintf(logger->logout, "23:59:59 -- LOG ROLLOVER --\n");
      logrollover = 1;
    }
    localtime_r(&now, &logger->logday);
    free(logger->datestr);
    strftime(timebuff, 20, "%F", &logger->logday);
    logger->datestr = strdup(timebuff);

    if (logger->logout) {
      fclose(logger->logout);
      logger->logout = NULL;
    }
  }

  if (logger->logout == NULL) {
    // Open the log file.
    snprintf(filename, 200, "%s-%s.log", logger->logname, logger->datestr);
    logger->logout = fopen(filename, "a");

    // If it was unable to initialize: Panic.
    if (logger->logout == NULL) {
      if (syslogger && logger != syslogger) {
        pthread_mutex_lock(&syslogger->logmutex);
        slog("Logging error: Unable to open '%s'!\n", filename);
        strerror_r(errno, filename, 100);
        slog("Logging error: %s\n", filename);
        pthread_mutex_unlock(&syslogger->logmutex);
      } else {
        printf("Logging error: Unable to open '%s'!\n", filename);
        strerror_r(errno, filename, 100);
        printf("Logging error: %s\n", filename);
      }
      pthread_mutex_unlock(&logger->logmutex);
      return;
    }
    if (logrollover) {
      fprintf(logger->logout, "00:00:00 -- LOG ROLLOVER --\n");
    }
  }

  strftime(timebuff, 20, "%T", &today);

  fprintf(logger->logout, "%s ", timebuff);

  vfprintf(logger->logout, fmt, args);
  fprintf(logger->logout, "\n");
  fflush(logger->logout);

  pthread_mutex_unlock(&logger->logmutex);
}

void
logger_init(const char *logpath) {
  pthread_mutexattr_init(&pth_recurse);
  pthread_mutexattr_settype(&pth_recurse, PTHREAD_MUTEX_RECURSIVE);
  syslogger = logger_new(logpath);
}

void
logger_shutdown() {
  logger_free(syslogger);
  syslogger = NULL;
}


static int ttylog = 1;

void
slog(const char *fmt, ...) {
  va_list args;
  if (syslogger) {
    pthread_mutex_lock(&syslogger->logmutex);
  }
  va_start(args, fmt);
  vllog(syslogger, fmt, args);
  va_end(args);
  if (ttylog) {
    if (isatty(1) || !syslogger) {
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
      printf("\n");
    } else {
      printf("Non-tty stdout detected. Logging only to %s",
             syslogger->logname);
      ttylog = 0;
    }
  }
  if (syslogger) {
    pthread_mutex_unlock(&syslogger->logmutex);
  }
}
