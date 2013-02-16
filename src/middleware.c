/* htserver.c
 *
 * The server wrapper around libevent for banana/etc.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "banana.h"
#include "htserver.h"
#include "logger.h"
#include "middleware.h"

MIDDLEWARE(mw_postonly) {
  if (htreq_ev(req)->type == EVHTTP_REQ_POST) {
    htreq_next(req);
  } else {
    htreq_not_allowed(req);
  }
}
