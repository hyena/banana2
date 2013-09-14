// mempool, memory pools for various objects.
//
#ifndef _MY_MEMPOOL_H_
#define _MY_MEMPOOL_H_

#include <stdarg.h>

#ifndef __attribute_malloc__
#define __attribute_malloc__ __attribute__((__malloc__))
#endif

struct _mempool;

typedef void (*freefunc)(void *ptr);

// Create
void  mp_set(struct _mempool **mp, const char *name, const void *ptr, freefunc release);
void * mp_calloc(struct _mempool **mp, const char *name, int size) __attribute_malloc__;
void * mp_strdup(struct _mempool **mp, const char *name, const char *val) __attribute_malloc__;
const char * mp_vsprintf(struct _mempool **mp, const char *name, const char *fmt, va_list args) __attribute_malloc__;
const char * mp_sprintf(struct _mempool **mp, const char *name, const char *fmt, ...) __attribute_malloc__;

// Get
const void * mp_get(struct _mempool **mp, const char *name) __attribute_malloc__;

// Destroy
void mp_free(struct _mempool **mp);

#endif /* _MY_MEMPOOL_H_ */
