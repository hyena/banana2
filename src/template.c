// template.h
//
// Banana's template engine
//
#include "banana.h"
#include "template.h"

typedef char * (*renderfunc)(struct citem *, struct htreq *)
#define RENDERER(x) char *x(struct citem *item, struct citem **ptr, struct htreq *req)
struct citem {
  renderfunc renderer;
  const char *contents;
  struct citem *next;
};

RENDERER(print) {
  return item->contents;
}

struct ctemplate {
  const char *path;
  time_t mtime;
  struct citem *tpl;
  struct ctemplate *next;
};

struct ctemplate *tplhead;

struct ctemplate *
template_load(const char *path, const char *filename) {
  char pathbuff[2048];
  struct ctemplate *tpl;
  snprintf(pathbuff, 2048, "%s/%s", path, filename);
  
  for (tpl = tplhead; tpl; tpl=tpl->next) {
    if (!strcmp(tpl->path, 
  }
}

const char *
template_eval(const char *path, const char *filename, struct htreq *req) {
  struct ctemplate *tpl = template_load(filename);
  if (!tpl) {
    char errbuff[2048];
    snprintf(errbuff, 2048, "Unknown template file: %s", filename);
    return htreq_strdup(req, errbuff);
  }
  return htreq_strdup(req, "I got a template!");
}

void
template_free(struct ctemplate *tpl) {
}
