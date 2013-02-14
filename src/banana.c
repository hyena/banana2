/* banana.c
 *
 */

#include "banana.h"

struct htserver *htserver;
struct config *bconfig;

HANDLER(mw_foo) {
  slog("foo middleware called");
  htreq_next(req);
}

HANDLER(notme) {
  slog("Notme called");
  htreq_send(req, "Welcome to Notme!");
}

void
banana_quit() {
  htserver_free(htserver);
  conf_free(bconfig);
}

int
main(int argc _unused_, char **argv _unused_) {
  struct htoptions options;
  bconfig = conf_read("config.txt");

  logger_init(bconf_get("log_path", "logs/banana"));
  slog("Banana initializing . . .");

  options.address = bconf_get("listen_host", "0.0.0.0");
  options.port = bconf_int("listen_port", 4080);
  options.http_signature = bconf_get("http_signature", "Banana MUSH Client");
  options.file_root = bconf_get("file_root", "public");

  htserver = htserver_new(&options);
  htserver_bind(htserver, "/notme", mw_foo, mw_foo, notme);
  slog("Starting Banana HTTP Server on port %d", options.port);
  htserver_start(htserver);

  return 0;
}
