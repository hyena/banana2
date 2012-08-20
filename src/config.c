/* config.c
 *
 *
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"

struct config {
  char name[CONF_MAX_LEN];
  char val[CONF_MAX_LEN];
  struct config *next;
};

#define MAX_BUFF (CONF_MAX_LEN * 3)

struct config *
conf_read(const char *pathname) {
  FILE *fin;
  char reader[MAX_BUFF];
  struct config *newconf = NULL, *config = NULL;
  fin = fopen(pathname, "r");
  if (fin == NULL) { return NULL; }
  char *i, *name, *val, *end;
  int skip = 0;

  while (fgets(reader, MAX_BUFF, fin) != NULL) {
    // We only care about lines that are short enough to
    // be config params.
    if (!strchr(reader, '\n')) {
      skip = 1;
      continue;
    }
    // If this line is a continuation of a too-long one, skip it.
    if (skip) {
      skip = 0;
      continue;
    }

    // Line isn't too long for config. Good.
    for (i = reader; *i && isspace(*i); i++);

    // Comments, blank lines, etc.
    if (!*i || !isalnum(*i)) continue;

    // We should have a name, now.
    for (name = i; i && *i && (isalnum(*i) || *i == '_'); i++);

    // Trim any spaces, foo : bar
    for (; *i && isspace(*i); i++) {
      *i = '\0';
    }
    // We require key : value
    if (*i != ':') continue;
    i++;

    // Now we find value
    for (; *i && isspace(*i); i++);

    val = i;
    for (end = i; *i; i++) {
      if (!isspace(*i)) {
        end = i;
      }
    }
    end++;
    *end = '\0';
    if (strlen(name) >= CONF_MAX_LEN || strlen(val) >= CONF_MAX_LEN) {
      continue;
    }
    // printf("Read in '%s':'%s'\n", name, val);
    newconf = malloc(sizeof(struct config));
    memset(newconf, 0, sizeof(struct config));
    strncpy(newconf->name, name, CONF_MAX_LEN);
    strncpy(newconf->val, val, CONF_MAX_LEN);
    newconf->next = config;
    config = newconf;
  }
  fclose(fin);
  return config;
}

struct config *
conf_free(struct config *tofree) {
  struct config *t;
  while (tofree) {
    t = tofree;
    tofree = t->next;
    free(t);
  }
  return NULL;
}

const char *
conf_get(struct config *conf, const char *name, const char *def) {
  while (conf && strcasecmp(conf->name, name)) {
    conf = conf->next;
  }
  if (conf) {
    return conf->val;;
  }
  return def;
}

int
conf_int(struct config *conf, const char *name, int def) {
  const char *v = conf_get(conf, name, NULL);
  if (!v) return def;
  return atoi(v);
}

int
conf_is(struct config *conf, const char *name, int def) {
  const char *v = conf_get(conf, name, NULL);
  if (!v) return def;
  if (!strcasecmp(v, "1")) return 1;
  if (!strcasecmp(v, "yes")) return 1;
  if (!strcasecmp(v, "y")) return 1;
  if (!strcasecmp(v, "on")) return 1;
  if (!strcasecmp(v, "true")) return 1;
  if (!strcasecmp(v, "t")) return 1;
  return 0;
}
