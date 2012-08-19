/* banana.c
 *
 */

#include "htserver.h"
#include "banana.h"

struct htserver *htserver;

HANDLER(mw_foo) {
  printf("mw_foo preprocessor called.\n");
  htreq_next(req);
  printf("mw_foo preprocessor called, post-next.\n");
}

HANDLER(notme) {
  printf("notme handler called.\n");
  htreq_not_found(req);
}

int
main(int argc _unused_, char **argv _unused_) {
  struct htoptions options;
  options.port = 4080;
  options.address = "0.0.0.0";
  options.verbose = 0;
  options.http_signature = "Walla walla Bing bang bamboozle";
  options.file_root = "public";
  options.file_index = "/index.html";

  htserver = htserver_new(&options);

  htserver_bind(htserver, "/notme", mw_foo, mw_foo, notme);

  htserver_start(htserver);

  return 0;
}
