/* em.c
 *
 * Eventmachine handler for banana
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "em.h"
#include "logger.h"

struct event_base *eventmachine;

struct event_base *
em_init() {
  slog("Initializing global eventmachine");
  eventmachine = event_base_new();
  return eventmachine;
}

void
em_start() {
  slog("Starting global event loop");
  event_base_dispatch(eventmachine);
}

void
em_stop() {
  slog("Ending global event loop");
  event_base_loopbreak(eventmachine);
}

struct thandler {
  timerhandler handler;
  const char *name;
  struct timeval *tv;
  void *ptr;
  struct event *ev;
};

void
loop_cb(evutil_socket_t fd __attribute__ ((__unused__)),
        short what __attribute__ ((__unused__)),
        void *arg) {
  struct thandler *thandler = (struct thandler *) arg;
  if (thandler->handler) {
    thandler->handler(thandler->ptr);
  }
  event_add(thandler->ev, thandler->tv);
}

void
em_loop(const char *name, int seconds, timerhandler handler, void *ptr) {
  struct thandler *thandler = malloc(sizeof(struct thandler));

  thandler->tv = malloc(sizeof(struct timeval));
  thandler->tv->tv_sec = seconds;
  thandler->tv->tv_usec = 0;

  thandler->handler = handler;
  thandler->name = name;
  thandler->ptr = ptr;

  thandler->ev = event_new(eventmachine,
                 -1,  // Not tied to a socket
                 EV_TIMEOUT, // Timeout type
                 loop_cb,
                 thandler);
  event_add(thandler->ev, thandler->tv);
}
