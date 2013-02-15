/* banana.c
 *
 */

#include "banana.h"
#include "em.h"

struct htserver *htserver = NULL;
struct config *bconfig = NULL;

HANDLER(mw_leak) {
  slog("leak middleware called");
}

HANDLER(mw_foo) {
  slog("foo middleware called");
  htreq_next(req);
}

HANDLER(page_notme) {
  slog("Notme called");
  htreq_send(req, "Welcome to Notme!");
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
  htserver_bind(htserver, "/notme", mw_foo, mw_foo, page_notme);
  htserver_bind(htserver, "/leak", mw_leak, page_notme);
  slog("Starting Banana HTTP Server on port %d", options.port);

  em_loop("htreq_check_unfree", 5, htreq_check_unfreed, NULL);

  // Start eventmachine.
  em_start();

  // Program control gets here when loopbreak is called.
  // Clean up.
  htserver_free(htserver);
  conf_free(bconfig);
  return 0;
}
