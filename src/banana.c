/* banana.c
 *
 */

#include "banana.h"
#include "em.h"
#include "_middleware.h"
#include "template.h"

struct htserver *htserver = NULL;
struct config *bconfig = NULL;

void
banana_quit() {
  em_stop();
}

#include "_page_defs.h"

void
bind_pages() {
#include "_page_gen.h"
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

  bind_pages();

  slog("Starting Banana HTTP Server on port %d", options.port);

  em_loop("htreq_check_unfree", 5, htreq_check_unfreed, NULL);
  em_loop("htreq_check_sessions", 5, htreq_check_sessions, NULL);

  // Start eventmachine.
  em_start();

  // Program control gets here when loopbreak is called.
  // Clean up.
  template_cleanup();
  htserver_free(htserver);
  conf_free(bconfig);
  return 0;
}
