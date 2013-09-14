/* banana.c
 *
 */

#include "banana.h"
#include "lib/em.h"
#include ".middleware.h"
#include "lib/template.h"

#include ".page_gen.h"

struct htserver *htserver = NULL;
struct config *bconfig = NULL;

void
banana_quit() {
  em_stop();
}

int
main(int argc _unused_, char **argv _unused_) {
  struct htoptions options;
  struct event_base *em;
  const char *opt_tvars = NULL;
  bconfig = conf_read("config.txt");

  logger_init(bconf_get("log_path", "logs/banana"));
  slog("Banana initializing . . .");

  options.address =        LISTEN_HOST;
  options.port =           LISTEN_PORT;
  options.http_signature = HTTP_SIGNATURE;
  options.file_root =      HTTP_ROOT;

  opt_tvars = bconf_get("template_vars", "tvars.txt");
  templatevars = conf_read(opt_tvars);

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
