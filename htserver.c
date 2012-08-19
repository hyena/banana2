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
#include "htserver.h"

#define _unused_ __attribute__ ((__unused__))

struct htserver {
  struct evhttp *server;
  struct event_base *eventmachine;
  const char *root;
  struct htoptions *options;
};

#define PATH_MAX_LEN 1024
struct mempair {
  void *ptr;
  freefunc release;
  struct mempair *next;
};

struct htreq {
  struct htserver *htserver;
  struct evhttp_request *evrequest;
  const char *uri;
  struct mempair *pool;
};

void
htreq_free(struct htreq *req) {
  struct mempair *n;
  while (req->pool) {
    n = req->pool;
    req->pool = n->next;
    n->release(n->ptr);
    free(n);
  }
  if (req->evrequest) {
    evhttp_request_free(req->evrequest);
  }
  free(req);
}

void *
htreq_calloc(struct htreq *req, int size, freefunc release) {
  struct mempair *n = malloc(sizeof(struct mempair));
  n->next = req->pool;
  n->release = release;
  n->ptr = malloc(size);
  memset(n->ptr, 0, size);
  return n->ptr;
}

void
htreq_not_found(struct htreq *req) {
  evhttp_send_error(req->evrequest, HTTP_NOTFOUND, "Four oh four, file not found.");
  req->evrequest = NULL;
  htreq_free(req);
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

  struct evkeyvalq *headers = evhttp_request_get_output_headers(req->evrequest);
  evhttp_add_header(headers, "Content-Type", "text/html; charset=UTF-8");
  evhttp_add_header(headers, "Server", "Hello");
  
  evhttp_send_reply(req->evrequest, HTTP_OK, "OK", buffer);
  req->evrequest = NULL;
  evbuffer_free(buffer);

  // Close htreq.
  htreq_free(req);
}

void
htreq_handle(struct evhttp_request *r, void *arg) {
  struct htserver *hts = (struct htserver *) arg;
  struct htreq *req = malloc(sizeof(struct htreq));
  memset(req, 0, sizeof(struct htreq));
  req->uri = evhttp_request_get_uri(r);
  evhttp_request_own(r);
  req->evrequest = r;
  req->htserver = hts;
  char path[PATH_MAX_LEN];

  printf("Checking uri: '%s'\n", req->uri);

  // First, check for a static file. (Not directory)
  if (hts->root) {
    struct stat st;
    snprintf(path, PATH_MAX_LEN, "%s/%s", hts->root, req->uri);
    printf("Checking path: '%s'\n", path);
    if (stat(path, &st) == 0) {
      if (!S_ISDIR(st.st_mode)) {
        htreq_read_file(req, path);
        return;
      }
    }
  }

  // Now check mounted handlers.

  // When all else fails, notfound it.
  htreq_not_found(req);
}

struct htserver *
htserver_new(struct htoptions *opts) {
  struct htserver *newht = malloc(sizeof(struct htserver));
  memset(newht, 0, sizeof(struct htserver));

  newht->eventmachine = event_base_new();
  newht->server = evhttp_new(newht->eventmachine);
  evhttp_bind_socket(newht->server, opts->address, opts->port);
  if (opts->file_root) {
    newht->root = opts->file_root;
  }
  evhttp_set_gencb(newht->server, htreq_handle, newht);
  return newht;
}

void
htserver_start(struct htserver *ht) {
  event_base_dispatch(ht->eventmachine);
}

void
htserver_stop(struct htserver *ht) {
  event_base_loopbreak(ht->eventmachine);
}

void
htserver_free(struct htserver *ht _unused_) {
}
