/* banana.c
 *
 */

#include "htserver.h"
#include "banana.h"

struct htserver *htserver;

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
  htserver_start(htserver);

  return 0;
}
