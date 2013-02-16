/* banana.c
 *
 */

#include "banana.h"
#include "middleware.h"
#include "em.h"

struct htserver *htserver = NULL;
struct config *bconfig = NULL;

MIDDLEWARE(mw_leak) {
  slog("leak middleware called");
}

MIDDLEWARE(mw_foo) {
  slog("foo middleware called");
  htreq_next(req);
}

PAGE(page_notme) {
  const char *foo = NULL;
  slog("Notme called");
  foo = htreq_session_get(req);
  if (foo) {
    slog("Notme session found '%s'", foo);
  }
  htreq_send(req, "Welcome to Notme!");
}

PAGE(page_foo) {
  slog("foo called");
  htreq_session_set(req, "Badonkadonk", NULL);
  htreq_send(req, "Foo! Your session is set!");
}

void
banana_quit() {
  em_stop();
}

int
main(int argc _unused_, char **argv _unused_) {
  struct htoptions options;
  struct event_base *em;
  bconfig = conf_read("config.txt");

  logger_init(bconf_get("log_path", "logs/banana"));
  slog("Banana initializing . . .");

  options.address =        LISTEN_HOST;
  options.port =           LISTEN_PORT;
  options.http_signature = HTTP_SIGNATURE;
  options.file_root =      HTTP_ROOT;

  em = em_init();
  // Add the http server to the eventmachine.
  htserver = htserver_new(&options, em);
  htserver_bind(htserver, "/notme", mw_session, page_notme);
  htserver_bind(htserver, "/foo", mw_postonly, mw_session, page_foo);
  htserver_bind(htserver, "/leak", mw_leak, page_notme);
  slog("Starting Banana HTTP Server on port %d", options.port);

  em_loop("htreq_check_unfree", 5, htreq_check_unfreed, NULL);
  em_loop("htreq_check_sessions", 5, htreq_check_sessions, NULL);

  // Start eventmachine.
  em_start();

  // Program control gets here when loopbreak is called.
  // Clean up.
  htserver_free(htserver);
  conf_free(bconfig);
  return 0;
}
