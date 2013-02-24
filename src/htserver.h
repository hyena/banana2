// General purpose header files
//
#ifndef _MY_EVHTTPSERVER_H_
#define _MY_EVHTTPSERVER_H_
#ifndef __attribute_malloc__
#define __attribute_malloc__ __attribute__((__malloc__))
#endif

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
#define MIDDLEWARE(x) void x(struct htreq *req _unused_, const char *uri _unused_)
#define PAGE(x, path, ...) MIDDLEWARE(x)

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
void htreq_not_allowed(struct htreq *req);
void htreq_read_file(struct htreq *req, const char *path);

extern const char *HT_COOKIE;
extern const char *HT_INTERNAL;
extern const char *HT_TEMP;
extern const char *HT_VAR;
extern const char *HT_TEMPLATE;

void *htreq_calloc(struct htreq *req, const char *category, const char *name, int size) __attribute_malloc__;
void *htreq_strdup(struct htreq *req, const char *category, const char *name, const char *val) __attribute_malloc__;
const char *htreq_sprintf(struct htreq *req, const char *category, const char *name, const char *fmt, ...);
void  htreq_mset(struct htreq *req, const char *category, const char *name, void *ptr, freefunc release);
#define htreq_set(req,cat,name,val) htreq_mset(ret, cat, name, val, NULL)
void *htreq_get(struct htreq *req, const char *category, const char *name) __attribute_malloc__;

void htreq_cookie_set(struct htreq *req, const char *name, const char *value, const char *flags);

#define SESSION_HANDLER(x) void x(void *ptr)
typedef void (*htsession_handler)(void *ptr);
void *htreq_session_get(struct htreq *req) __attribute_malloc__;
void  htreq_session_set(struct htreq *req, void *ptr, htsession_handler onfree);
struct evhttp_request *htreq_ev(struct htreq *req);

MIDDLEWARE(mw_session);

void  htreq_check_unfreed();
void  htreq_check_sessions();

void htreq_end(struct htreq *req);

#endif /* _MY_EVHTTPSERVER_H_ */
