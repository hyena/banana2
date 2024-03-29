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
#include <event2/buffer.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "banana.h"
#include "htserver.h"
#include "mempool.h"
#include "page.h"
#include "logger.h"

#define _unused_ __attribute__ ((__unused__))

#define MAX_HANDLERS 30
#define SESSION_KEY_LENGTH 30

struct _handler {
  const char *path;
  int         pathlen;
  reqhandler *funs;
  int         count;
  struct _handler *next;
};

struct htserver {
  struct evhttp *server;
  struct event_base *eventmachine;
  const char *root;
  struct htoptions *options;
  struct _handler *preprocessor, *preprocessortail;
  struct _handler *handler, *handlertail;
};

#define PATH_MAX_LEN 1024
#define MAX_TEMPLATE_POOLS 8
struct htreq {
  char   path[PATH_MAX_LEN];
  struct htserver *htserver;
  struct evhttp_request *evrequest;
  struct _handler  *handlers[MAX_HANDLERS];
  int    handler_count;
  int    handler_index;
  int    handler_subindex;
  int    longpoll;
  struct event *evloop;
  const char *uri;
  struct {
    struct _mempool *tvals;
    struct _mempool *internal;
    struct _mempool *cookies;
  } mp;
  struct _mempool **tpools[MAX_TEMPLATE_POOLS];
  struct ht_session *session;
};

struct htreq_list {
  struct htreq_list *next;
  struct htreq *req;
  time_t addtime;
};

static struct htreq_list *_htreq_track = NULL;

void
track_htreq(struct htreq *req) {
  struct htreq_list *tracker =
      mp_calloc(&(req->mp.internal), "tracker", sizeof(struct htreq_list));
  tracker->req = req;
  tracker->next = _htreq_track;
  tracker->addtime = time(NULL);
  _htreq_track = tracker;
}

void
untrack_htreq(struct htreq *req) {
  struct htreq_list **ptr = &_htreq_track;
  while (*ptr) {
    if ((*ptr)->req == req) {
      *ptr = (*ptr)->next;
      break;
    } else {
      ptr = &(*ptr)->next;
    }
  }
}

void
htreq_check_unfreed() {
  struct htreq_list *ptr = _htreq_track;
  time_t now = time(NULL);
  time_t expiry = now - 20;
  time_t longpoll_expiry = now - LONGPOLL_TIMEOUT - 20;
  struct htreq *req;

  while (ptr) {
    req = ptr->req;
    if (req->longpoll) {
      if (ptr->addtime < longpoll_expiry) {
        slog("WARN: Unreleased longpoll htreq: %s (%d seconds old) - Freeing", ptr->req->uri, now-ptr->addtime);
        htreq_not_found(req);
      }
    } else if (ptr->addtime < expiry) {
      slog("WARN: Unreleased htreq: %s (%d seconds old) - Freeing", ptr->req->uri, now-ptr->addtime);
      htreq_not_found(req);
    }
    ptr = ptr->next;
  }
}

void htreq_next_cb(evutil_socket_t socket _unused_, short flags _unused_, void *arg);

void
htreq_end(struct htreq *req) {
  untrack_htreq(req);
  mp_free(&req->mp.cookies);
  mp_free(&req->mp.tvals);
  mp_free(&req->mp.internal);
  if (req->evrequest) {
    evhttp_request_free(req->evrequest);
  }
  if (req->evloop) {
    event_free(req->evloop);
  }
  free(req);
}

void
htreq_cookie_set(struct htreq *req,
                 const char *name, const char *value,
                 const char *flags) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(req->evrequest);
  char cookieval[2048];
  char *encoded = evhttp_encode_uri(name);
  if (flags) {
    snprintf(cookieval, 2048, "%s=%s; %s", encoded, value, flags);
  } else {
    snprintf(cookieval, 2048, "%s=%s; Path=/", encoded, value);
  }
  free(encoded);
  evhttp_add_header(headers, "Set-Cookie", cookieval);
}

void
htreq_set_header(struct htreq *req, const char *name, const char *val) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(req->evrequest);
  evhttp_add_header(headers, name, val);
}

void
_htreq_set_headers(struct htreq *req) {
  htreq_set_header(req, "Server", "Banana HTML to MUSH gateway");
}

void
_htreq_set_cookies(struct htreq *req) {
  const char *cookies;
  char *cval = NULL;
  char *nptr, *vptr, *next;
  char *decoded;
  struct evkeyvalq *headers = evhttp_request_get_input_headers(req->evrequest);
  cookies = evhttp_find_header(headers, "Cookie");
  if (cookies) {
    cval = strdup(cookies);
    nptr = cval;
    while (nptr) {
      while (isspace(*nptr)) nptr++;
      if (!*nptr) break;
      vptr = strchr(nptr, '=');
      if (!vptr) break;
      *(vptr++) = '\0';
      next = strchr(vptr, ';');
      if (next) *(next++) = '\0';
      decoded = evhttp_decode_uri(vptr);
      nptr = mp_strdup(&req->mp.internal, "cookiename", nptr);
      mp_strdup(&req->mp.cookies, nptr, decoded);
      free(decoded);
      nptr = next;
    }
    free(cval);
  }
}

struct ht_session {
  char key[SESSION_KEY_LENGTH+1];
  void *sessiondata;
  time_t expiry;
  struct ht_session *next;
  htsession_handler onfree;
};

struct ht_session *session_head;

void *
htreq_session_get(struct htreq *req) {
  if (req->session) {
    return req->session->sessiondata;
  } else {
    slog("htreq_session_get called without a session set?");
    return NULL;
  }
}

void
htreq_session_set(struct htreq *req, void *ptr, htsession_handler onfree) {
  if (req->session) {
    req->session->onfree = onfree;
    req->session->sessiondata = ptr;
  } else {
    slog("htreq_session_set called without a session set?");
  }
}

void
htreq_check_sessions() {
  struct ht_session **htsp = &session_head;
  struct ht_session *hts;
  time_t now = time(NULL);
  while (htsp && *htsp) {
    hts = *htsp;
    if (hts->expiry < now) {
      *htsp = hts->next;
      slog("Expiring session %s", hts->key);
      if (hts->onfree) {
        hts->onfree(hts->sessiondata);
      }
      free(hts);
    } else {
      htsp = &(hts->next);
    }
  }
}

MIDDLEWARE(mw_session) {
  const char *cookieval = mp_get(&req->mp.cookies, "sess");
  int i;
  time_t expiry = time(NULL) + SESSION_TIMEOUT;
  struct ht_session *hts;
  if (cookieval) {
    for (hts = session_head; hts; hts = hts->next) {
      if (!strcmp(hts->key, cookieval)) {
        req->session = hts;
        hts->expiry = expiry;
        htreq_next(req);
        return;
      }
    }
  }

  hts = malloc(sizeof(struct ht_session));
  memset(hts, 0, sizeof(struct ht_session));
  for (i = 0; i < SESSION_KEY_LENGTH; i++) {
    hts->key[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand()%62];
  }
  hts->key[i] = '\0';
  slog("Creating session %s", hts->key);
  hts->expiry = expiry;
  hts->next = session_head;
  session_head = hts;
  htreq_cookie_set(req, "sess", hts->key, NULL);
  req->session = hts;
  htreq_next(req);
}

struct evhttp_request *htreq_ev(struct htreq *req) {
  return req->evrequest;
}

void
htreq_not_found(struct htreq *req) {
  _htreq_set_headers(req);
  if (req->evrequest) {
    evhttp_send_error(req->evrequest, HTTP_NOTFOUND, "Four oh four, file not found.");
    req->evrequest = NULL;
  }
  htreq_end(req);
}

void
htreq_not_allowed(struct htreq *req) {
  _htreq_set_headers(req);
  if (req->evrequest) {
    evhttp_send_error(req->evrequest, 403, "Four oh Three, you're not allowed.");
    req->evrequest = NULL;
  }
  htreq_end(req);
}

void
htreq_send(struct htreq *req, const char *ctype, const char *body) {
  _htreq_set_headers(req);
  htreq_set_header(req, "Content-Type", ctype);
  struct evbuffer *buffer;
  buffer = evbuffer_new();
  evbuffer_add(buffer, body, strlen(body));

  evhttp_send_reply(req->evrequest, HTTP_OK, "OK", buffer);
  req->evrequest = NULL;
  evbuffer_free(buffer);
  htreq_end(req);
}

void
htreq_t_send(struct htreq *req, const char *templatename) {
  const char *body, *page;

  _htreq_set_headers(req);
  htreq_set_header(req, "Content-Type", CTYPE_HTML);

  body = template_eval(templatename, req);
  mp_set(&req->mp.tvals, "layout-body", body, NULL);

  page = template_eval("base.html", req);

  struct evbuffer *buffer;
  buffer = evbuffer_new();
  evbuffer_add(buffer, page, strlen(page));

  evhttp_send_reply(req->evrequest, HTTP_OK, "OK", buffer);
  req->evrequest = NULL;
  evbuffer_free(buffer);
  htreq_end(req);
}

void
htreq_t_add_pool(struct htreq *req, struct _mempool **mp) {
  int i;
  for (i=0; i < MAX_TEMPLATE_POOLS; i++) {
    if (req->tpools[i] == NULL) {
      req->tpools[i] = mp;
    }
  }
}

const char *
htreq_t_get(struct htreq *req, const char *name) {
  int i;
  const char *ret;
  for (i=0; i < MAX_TEMPLATE_POOLS; i++) {
    if (req->tpools[i] == NULL) break;
    ret = mp_get(req->tpools[i], name);
    if (ret) return ret;
  }
  return NULL;
}

const char *
htreq_t_sprintf(struct htreq *req, const char *name, const char *fmt, ...) {
  const char *ptr;
  va_list args;
  va_start(args, fmt);
  ptr = mp_vsprintf(&req->mp.tvals, name, fmt, args);
  va_end(args);
  return ptr;
}

char *
htreq_t_calloc(struct htreq *req, const char *name, int len) {
  return mp_calloc(&req->mp.tvals, name, len);
}

const char *
htreq_t_strdup(struct htreq *req, const char *name, const char *str) {
  return mp_strdup(&req->mp.tvals, name, str);
}


void
htreq_read_file(struct htreq *req, const char *path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    htreq_not_found(req);
    return;
  }

  struct evbuffer *buffer;
  buffer = evbuffer_new();

  if (st.st_size != 0) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
      evbuffer_free(buffer);
      htreq_not_found(req);
      return;
    }
    evbuffer_read(buffer, fd, st.st_size);
    close(fd);
  }

  _htreq_set_headers(req);
  
  evhttp_send_reply(req->evrequest, HTTP_OK, "OK", buffer);
  req->evrequest = NULL;
  evbuffer_free(buffer);

  // Close htreq.
  htreq_end(req);
}

void
htreq_handle(struct evhttp_request *r, void *arg) {
  struct htserver *hts = (struct htserver *) arg;
  struct htreq *req = malloc(sizeof(struct htreq));
  memset(req, 0, sizeof(struct htreq));
  evhttp_request_own(r);
  track_htreq(req);
  req->uri = evhttp_request_get_uri(r);
  req->evrequest = r;
  req->htserver = hts;
  req->handler_count = 0;
  req->handler_index = 0;
  req->handler_subindex = 0;
  htreq_t_add_pool(req, &req->mp.tvals);
  snprintf(req->path, PATH_MAX_LEN, "%s/%s", hts->root, req->uri);
  _htreq_set_cookies(req);

  // First, check for a static file. (Not directory)
  if (hts->root) {
    struct stat st;
    if (stat(req->path, &st) == 0) {
      if (!S_ISDIR(st.st_mode)) {
        htreq_read_file(req, req->path);
        return;
      }
    }
  }

  // Now check mounted handlers.
  //
  // Load in preprocessors.
  struct _handler *cur;
  for (cur = hts->preprocessor; cur != NULL; cur = cur->next) {
    if (req->handler_count >= MAX_HANDLERS) {
      printf("Too many handlers for uri %s?\n", req->uri);
      htreq_not_found(req);
      return;
    }
    req->handlers[req->handler_count++] = cur;
  }

  // Look for a matching path handler.
  for (cur = hts->handler; cur != NULL; cur = cur->next) {
    if (!strncmp(cur->path, req->uri, cur->pathlen)) {
      break;
    }
  }

  if (cur) {
    if (req->handler_count >= MAX_HANDLERS) {
      printf("Too many handlers for uri %s?\n", req->uri);
      htreq_not_found(req);
      return;
    }
    req->handlers[req->handler_count++] = cur;
    req->evloop = event_new(req->htserver->eventmachine,
                            -1, // Not tied to a socket.
                            0,  // Activate immediately.
                            htreq_next_cb,
                            (void *) req);
    htreq_next(req);
  } else {
    // When all else fails, notfound it.
    htreq_not_found(req);
  }
}

void
htreq_next_cb(evutil_socket_t socket _unused_, short flags _unused_, void *arg) {
  struct htreq *req = (struct htreq *) arg;
  if (!req
      || req->handler_index >= req->handler_count
      || req->handler_subindex >= req->handlers[req->handler_index]->count) {
    printf("Err?\n");
    htreq_not_found(req);
    return;
  }

  struct _handler *cur = req->handlers[req->handler_index];

  reqhandler fun = cur->funs[req->handler_subindex];

  if (req->handler_subindex > cur->count) {
    req->handler_index++;
    req->handler_subindex = 0;
  } else {
    req->handler_subindex++;
  }

  fun(req, req->uri);
}

void
htreq_next(struct htreq *req) {
  // Push this request onto the next event loop.
  // (This has a bonus of removing most stack usage?)
  event_active(req->evloop, 0, 0);
}

void
htserver_bind_real(struct htserver *hts, const char *path, ...) {
  va_list args;
  int count = 0;
  reqhandler handler;
  va_start(args, path);
  while ((handler = va_arg(args, reqhandler)) != NULL) {
    count++;
  }
  va_end(args);

  struct _handler *h = malloc(sizeof(struct _handler));
  h->path = path;
  h->pathlen = strlen(path);
  h->funs = malloc(sizeof(reqhandler) * count);
  h->count = 0;
  h->next = NULL;

  va_start(args, path);
  while ((handler = va_arg(args, reqhandler)) != NULL) {
    h->funs[h->count++] = handler;
  }
  va_end(args);

  if (hts->handlertail == NULL) {
    hts->handler = h;
    hts->handlertail = h;
  } else {
    hts->handlertail->next = h;
    hts->handlertail = h;
  }
}

void
htserver_preprocessor_real(struct htserver *hts, ...) {
  va_list args;
  int count = 0;
  reqhandler handler;
  va_start(args, hts);
  while ((handler = va_arg(args, reqhandler)) != NULL) {
    count++;
  }
  va_end(args);

  struct _handler *h = malloc(sizeof(struct _handler));
  h->path = "preprocessor";
  h->funs = malloc(sizeof(reqhandler) * count);
  h->count = 0;
  h->next = NULL;

  va_start(args, hts);
  while ((handler = va_arg(args, reqhandler)) != NULL) {
    h->funs[h->count++] = handler;
  }
  va_end(args);

  if (hts->preprocessortail == NULL) {
    hts->preprocessor = h;
    hts->preprocessortail = h;
  } else {
    hts->preprocessortail->next = h;
    hts->preprocessortail = h;
  }
}

struct htserver *
htserver_new(struct htoptions *opts, struct event_base *eventmachine) {
  struct htserver *newht = malloc(sizeof(struct htserver));
  memset(newht, 0, sizeof(struct htserver));

  newht->eventmachine = eventmachine;
  newht->server = evhttp_new(newht->eventmachine);
  evhttp_bind_socket(newht->server, opts->address, opts->port);
  if (opts->file_root) {
    newht->root = opts->file_root;
  }
  evhttp_set_gencb(newht->server, htreq_handle, newht);
  return newht;
}

void
htserver_free(struct htserver *ht _unused_) {
  // TODO: this.
}
