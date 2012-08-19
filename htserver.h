// General purpose header files
//
#ifndef _MY_EVHTTPSERVER_H_
#define _MY_EVHTTPSERVER_H_
#include <sys/stat.h>

// Libevent header files
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>

struct htserver;
struct htreq;

// Libsrvr options
struct htoptions {
  char *address;
  int   port;
  int   verbose;
  char *http_signature;
  char *file_root;
  char *file_index;
};

struct htserver *htserver_new(struct htoptions *opts);
void htserver_free(struct htserver *);

// htserver_start doesn't return until htserver_stop is called
// elsewhere.
void htserver_start(struct htserver *);
void htserver_stop(struct htserver *);

typedef void (*freefunc)(void *ptr);

void *htreq_calloc(struct htreq *req, int size, freefunc release) __attribute_malloc__;
void htreq_free(struct htreq *req);

#endif /* _MY_EVHTTPSERVER_H_ */
