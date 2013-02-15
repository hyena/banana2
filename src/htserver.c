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
#include "htserver.h"
#include "logger.h"

#define _unused_ __attribute__ ((__unused__))

#define MAX_HANDLERS 30

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
struct _mempair {
  const char *name;
  void *ptr;
  freefunc release;
  struct _mempair *next;
};

struct htreq {
  char   path[PATH_MAX_LEN];
  struct htserver *htserver;
  struct evhttp_request *evrequest;
  struct _handler  *handlers[MAX_HANDLERS];
  int    handler_count;
  int    handler_index;
  int    handler_subindex;
  struct event *evloop;
  const char *uri;
  struct _mempair *pool;
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
      htreq_calloc(req, "tracker", sizeof(struct htreq_list));
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
htreq_list_unfreed() {
  struct htreq_list *ptr = _htreq_track;
  time_t now = time(NULL);
  while (ptr) {
    slog("Unreleased htreq: %s (%d seconds old)", ptr->req->uri, now-ptr->addtime);
    ptr = ptr->next;
  }
}

void htreq_next_cb(evutil_socket_t socket _unused_, short flags _unused_, void *arg);

void
htreq_end(struct htreq *req) {
  struct _mempair *n;
  untrack_htreq(req);
  while (req->pool) {
    n = req->pool;
    req->pool = n->next;
    if (n->release) {
      n->release(n->ptr);
    }
    free(n);
  }
  if (req->evrequest) {
    evhttp_request_free(req->evrequest);
  }
  if (req->evloop) {
    event_free(req->evloop);
  }
  free(req);
}

void *
htreq_calloc(struct htreq *req, const char *name, int size) {
  void *ptr = malloc(size);
  memset(ptr, 0, size);
  htreq_mset(req, name, ptr, free);
  return ptr;
}

void
htreq_mset(struct htreq *req, const char *name, void *val, freefunc release) {
  struct _mempair *n = malloc(sizeof(struct _mempair));
  n->name = name;
  n->next = req->pool;
  n->release = release;
  n->ptr = val;
  req->pool = n;
}

void
htreq_set(struct htreq *req, const char *name, void *val) {
  struct _mempair *n = malloc(sizeof(struct _mempair));
  n->name = name;
  n->next = req->pool;
  n->release = NULL;
  n->ptr = val;
  req->pool = n;
}

void *
htreq_get(struct htreq *req, const char *name) {
  struct _mempair *n = req->pool;
  while (n) {
    if (n->name && !strcmp(n->name, name)) {
      return n->ptr;
    }
    n = n->next;
  }
  return NULL;
}

void
_htreq_set_headers(struct htreq *req) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(req->evrequest);
  // TODO: File mime type detection.
  evhttp_add_header(headers, "Content-Type", "text/html; charset=UTF-8");
  evhttp_add_header(headers, "Server", "Hello");
}

void
htreq_not_found(struct htreq *req) {
  _htreq_set_headers(req);
  evhttp_send_error(req->evrequest, HTTP_NOTFOUND, "Four oh four, file not found.");
  req->evrequest = NULL;
  htreq_end(req);
}

void
htreq_send(struct htreq *req, const char *body) {
  _htreq_set_headers(req);
  struct evbuffer *buffer;
  buffer = evbuffer_new();
  evbuffer_add(buffer, body, strlen(body));

  evhttp_send_reply(req->evrequest, HTTP_OK, "OK", buffer);
  req->evrequest = NULL;
  evbuffer_free(buffer);
  htreq_end(req);
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
  snprintf(req->path, PATH_MAX_LEN, "%s/%s", hts->root, req->uri);

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

  va_start(args, NULL);
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
