// General purpose header files
//
#ifndef _MY_EVHTTPSERVER_H_
#define _MY_EVHTTPSERVER_H_
#include <sys/stat.h>

// Libevent header files
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>

#define _unused_ __attribute__ ((__unused__))
struct htserver;
struct htreq;

// Libsrvr options
struct htoptions {
  const char *address;
  int   port;
  const char *http_signature;
  const char *file_root;
};

struct htserver *htserver_new(struct htoptions *opts, struct event_base *em);
void   htserver_free(struct htserver *);

typedef void (*reqhandler)(struct htreq *req, const char *uri);
#define HANDLER(x) void x(struct htreq *req _unused_, const char *uri _unused_)

void htserver_bind_real(struct htserver *hts, const char *path, ...);
#define htserver_bind(hts, path, args...) \
            htserver_bind_real(hts, path, ## args, NULL)

void htserver_preprocessor_real(struct htserver *hts, ...);
#define htserver_preprocessor(hts, args...) \
            htserver_preprocessor_real(hts, ## args, NULL)


typedef void (*freefunc)(void *ptr);

void htreq_next(struct htreq *req);
void htreq_send(struct htreq *req, const char *body);

void htreq_not_found(struct htreq *req);
void htreq_read_file(struct htreq *req, const char *path);

void *htreq_calloc(struct htreq *req, const char *name, int size) __attribute_malloc__;
void  htreq_mset(struct htreq *req, const char *name, void *ptr, freefunc release);
void  htreq_set(struct htreq *req, const char *name, void *ptr);
void *htreq_get(struct htreq *req, const char *name) __attribute_malloc__;

void  htreq_check_unfreed();

void htreq_end(struct htreq *req);

#endif /* _MY_EVHTTPSERVER_H_ */
