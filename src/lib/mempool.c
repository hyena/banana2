/* mempool.c
 *
 * Manage key:value stores.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include "lib/mempool.h"

#define _unused_ __attribute__ ((__unused__))

struct _mempool {
  const char *category;
  const char *name;
  const void *ptr;
  freefunc release;
  struct _mempool *next;
};

void
mp_free(struct _mempool **mp) {
  struct _mempool *n;
  for (n = *mp; n; n = *mp) {
    *mp = n->next;
    if (n->release) {
      n->release((void *) n->ptr);
    }
    free(n);
  }
}

void
mp_set(struct _mempool **mp, const char *name, const void *val, freefunc release) {
  struct _mempool *n = malloc(sizeof(struct _mempool));
  n->name = name;
  n->next = *mp;
  n->release = release;
  n->ptr = val;
  *mp = n;
}

void *
mp_calloc(struct _mempool **mp, const char *name, int size) {
  void *ptr = malloc(size);
  memset(ptr, 0, size);
  mp_set(mp, name, ptr, free);
  return ptr;
}

void *
mp_strdup(struct _mempool **mp, const char *name, const char *val) {
  void *ptr = strdup(val);
  mp_set(mp, name, ptr, free);
  return ptr;
}

const char *
mp_vsprintf(struct _mempool **mp, const char *name, const char *fmt, va_list args) {
  char *ptr;
  vasprintf(&ptr, fmt, args);
  mp_set(mp, name, ptr, free);
  return ptr;
}

const char *
mp_sprintf(struct _mempool **mp, const char *name, const char *fmt, ...) {
  va_list args;
  const char *ptr;
  va_start(args, fmt);
  ptr = mp_vsprintf(mp, name, fmt, args);
  va_end(args);
  mp_set(mp, name, ptr, free);
  return ptr;
}

const void *
mp_get(struct _mempool **mp, const char *name) {
  struct _mempool *n = *mp;
  while (n) {
    if (n->name && !strcmp(n->name, name)) {
      return n->ptr;
    }
    n = n->next;
  }
  return NULL;
}
