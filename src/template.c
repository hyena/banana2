// template.h
//
// Banana's template engine
//
#include "banana.h"
#include "logger.h"
#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

struct citem;
struct render_item;

typedef void (*renderfunc)(struct htreq *req,
                           struct citem *item,
                           struct render_item ***ri);
#define RENDERER(x) void x(struct htreq *req _unused_, \
                           struct citem *item _unused_, \
                           struct render_item ***ri _unused_)

RENDERER(render_template);

// [[IF foobar]]
// Text
// [[ENDIF]]
struct citem {
  // RENDERER(if)
  renderfunc renderer;
  // Name of function (e.g: IF)
  char *name;
  // The content after function (e.g: foobar)
  // Or, for string type, it's the body of the string.
  char *contents;
  // a list of citems within this, in the event
  // of if, while, etc.
  struct citem *cval;
  // Next in the list.
  struct citem *next;
};

struct render_item {
  const char *val;
  struct render_item *next;
};

void
append(struct render_item ***ri, const char *val) {
  struct render_item *rn = malloc(sizeof(struct render_item));
  memset(rn, 0, sizeof(struct render_item));
  (**ri) = rn;
  rn->val = val;
  (*ri) = &(rn->next);
}

RENDERER(tpl_print) {
  append(ri, item->contents);
}

RENDERER(tpl_if) {
  append(ri, "IF (");
  append(ri, item->contents);
  append(ri, ") {");
  render_template(req, item->cval, ri);
  append(ri, "}");
}

RENDERER(tpl_unless) {
  append(ri, "UNLESS (");
  append(ri, item->contents);
  append(ri, ") {");
  render_template(req, item->cval, ri);
  append(ri, "}");
}

RENDERER(tpl_include) {
  append(ri, "INCLUDE (");
  append(ri, item->contents);
  append(ri, ")");
}

RENDERER(tpl_eq) {
  append(ri, "= (");
  append(ri, item->contents);
  append(ri, ")");
}

struct rendertype {
  const char *name;
  const char *end;
  renderfunc renderer;
};

struct rendertype renderers[] = {
  { "IF", "ENDIF", tpl_if },
  { "UNLESS", "ENDUNLESS", tpl_unless },
  { "INCLUDE", NULL, tpl_include },
  { "=", NULL, tpl_eq },
  { NULL, NULL, NULL }
};

struct ctemplate {
  const char *path;
  // body contains the entire file, but is mangled. All citems/etc point to
  // strings inside the body.
  // sval, contents, name should never be free()'d.
  char *body;
  time_t mtime;
  struct citem *items;
  struct ctemplate *next;
};

struct ctemplate *tplhead;

struct citem err_citem = { tpl_print, NULL, NULL, NULL, NULL };

void
citem_free(struct citem *ci) {
  struct citem *cn;
  while (ci) {
    cn = ci->next;
    if (ci->cval) {
      citem_free(ci->cval);
    }
    free(ci);
    ci = cn;
  }
}

#define TYPE_STR 0
#define TYPE_PARAM 1
struct build_citem {
  int type;
  char *ptr;
  struct build_citem *next;
};

struct build_citem *
build_citems(char *p) {
  struct build_citem *ret = NULL;
  struct build_citem **ptr = &ret;
  struct build_citem *new = NULL;
  char *pstart, *pend, *i;

  // Break it apart into STRING PARAM STRING PARAM STRING PARAM.
  while (p && *p) {
    new = malloc(sizeof(struct build_citem));
    memset(new, 0, sizeof(struct build_citem));
    new->type = TYPE_STR;
    new->ptr = p;
    *ptr = new;
    ptr = &(new->next);
    pstart = strstr(p, "[[");

    // If there's no [[..]], we're done.
    if (!pstart) break;
    pend = strstr(pstart, "]]");
    if (!pend) break;
    if (new->ptr != pstart) {
      new = malloc(sizeof(struct build_citem));
      memset(new, 0, sizeof(struct build_citem));
      *ptr = new;
      ptr = &(new->next);
    }
    *(pstart++) = '\0'; // [
    *(pstart++) = '\0'; // [
    while (isspace(*pstart)) {
      *(pstart++) = '\0';
    }
    // Strip right side spaces.
    for (i = pend-1; isspace(*i); i--) {
      *(i) = '\0';
    }
    *(pend++) = '\0'; // ]
    *(pend++) = '\0'; // ]
    new->type = TYPE_PARAM;
    new->ptr = pstart;
    p = pend;
  }
  return ret;
}

const char *
find_renderer(char *param, struct citem *ci) {
  int i;
  char *p;
  ci->name = param;
  p = ci->name;
  while (*p && !isspace(*p)) p++;
  while (isspace(*p)) { *(p++) = '\0'; }
  if (*p) {
    ci->contents = p;
  }
  for (i=0; renderers[i].name; i++) {
    if (!strcasecmp(renderers[i].name, param)) {
      ci->renderer = renderers[i].renderer;
      return renderers[i].end;
    }
  }
  return NULL;
}

struct citem *
create_citems(struct build_citem **build, const char *end) {
  struct citem *ret = NULL;
  struct build_citem *bp;
  struct citem **ptr = &ret;
  struct citem *ci;
  const char *subend = NULL;

  while (build && *build) {
    bp = *build;
    *build = bp->next;
    ci = malloc(sizeof(struct citem));
    memset(ci, 0, sizeof(struct citem));
    if (bp->type == TYPE_STR) {
      ci->name = NULL;
      ci->contents = bp->ptr;
      ci->renderer = tpl_print;
    } else {
      if (end && !strcasecmp(bp->ptr, end)) {
        break;
      }
      subend = find_renderer(bp->ptr, ci);

      // Recurse?
      if (subend) {
        ci->cval = create_citems(build, subend);
      }
    }
    *ptr = ci;
    ptr = &(ci->next);
  }
  return ret;
}

struct citem *
read_items(char *p) {
  struct build_citem *build = build_citems(p);
  struct build_citem *bn;
  struct citem *ret;
  bn = build;
  ret = create_citems(&bn, NULL);
  while (build) {
    bn = build->next;
    free(build);
    build = bn;
  }
  return ret;
}

struct citem *
template_load(const char *path, const char *filename) {
  char pathbuff[2048];
  FILE *fin;
  struct ctemplate *tpl;
  struct stat sb;
  int ret;
  snprintf(pathbuff, 2048, "%s/%s", path, filename);
  ret = stat(pathbuff, &sb);

  if (ret < 0) {
    slog("Unable to render template '%s': '%s'", pathbuff, strerror(errno));
    err_citem.contents = strerror(errno);
    return &err_citem;
  }
  
  for (tpl = tplhead; tpl; tpl=tpl->next) {
    if (!strcmp(tpl->path, pathbuff)) {
      if (tpl->mtime == sb.st_mtime) {
        return tpl->items;
      }
      break;
    }
  }
  if (!tpl) {
    tpl = malloc(sizeof(struct ctemplate));
    memset(tpl, 0, sizeof(struct ctemplate));
    tpl->next = tplhead;
    tplhead = tpl;
    tpl->path = strdup(pathbuff);
  } else {
    citem_free(tpl->items);
    free(tpl->body);
    tpl->items = NULL;
    tpl->body = NULL;
  }
  tpl->mtime = sb.st_mtime;
  tpl->body = malloc(sb.st_size);

  fin = fopen(pathbuff, "r");
  fread(tpl->body, 1, sb.st_size, fin);
  fclose(fin);

  // We now have the body.
  tpl->items = read_items(tpl->body);
  return tpl->items;
}

RENDERER(render_template) {
  while (item) {
    if (item->renderer) {
      item->renderer(req, item, ri);
    }
    item = item->next;
  }
}

const char *
template_eval(const char *path, const char *filename, struct htreq *req) {
  struct citem *items = template_load(path, filename);
  struct render_item *ritems, *ri, **rptr;
  char *ret;
  int len;
  if (!items) {
    slog("template_eval: '%s/%s' is an empty file?", path, filename);
    return htreq_strdup(req, HT_INTERNAL, "tplerror", "");
  }
  ritems = NULL;
  rptr = &ritems;
  render_template(req, items, &rptr);
  for (len = 0, ri = ritems; ri; ri = ri->next) {
    len += strlen(ri->val);
  }
  len++; // \0;
  ret = htreq_calloc(req, HT_INTERNAL, "template_result", len);
  for (ri = ritems; ri; ri = ri->next) {
    strcat(ret, ri->val);
  }
  return ret;
}
