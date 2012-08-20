/* banana.c
 *
 */

#include "htserver.h"
#include "banana.h"
#include "config.h"

struct htserver *htserver;

HANDLER(mw_foo) {
  printf("mw_foo preprocessor called.\n");
  htreq_next(req);
  printf("mw_foo preprocessor called, post-next.\n");
}

HANDLER(notme) {
  printf("notme handler called.\n");
  htreq_send(req, "Welcome to Notme!");
}

int
main(int argc _unused_, char **argv _unused_) {
  struct htoptions options;
  struct config *config = conf_read("config.txt");
  options.address = conf_get(config, "listen_host", "0.0.0.0");
  options.port = conf_int(config, "listen_port", 4080);
  options.http_signature = conf_get(config, "http_signature", "Banana MUSH Client");
  options.file_root = conf_get(config, "file_root", "public");

  htserver = htserver_new(&options);
  htserver_bind(htserver, "/notme", mw_foo, mw_foo, notme);
  htserver_start(htserver);

  return 0;
}
