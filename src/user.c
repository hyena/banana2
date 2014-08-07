/* user.c
 */

#include "user.h"
#include <stdio.h>
#include <ctype.h>
#define MAX_NAME_LEN 20
#define MAX_PATH_LEN 40

struct user {
  const char *name[MAX_NAME_LEN];
  time_t last_activity;

  const char *path[MAX_PATH_LEN];
  User *next;
};

User *head = NULL;

User *
user_locate(const char *login) {
  User *h;
  for (h = head; h && strcmp(login, h->name); h = h->next);
  return n;
}

int
valid_user_name(const char *n) {
  char x;
  int i;

  for (i=0; n[i]; i++) {
    x = n[i];
    if (isalnum(x)) continue;
    if (x == '.') continue;
    if (x == '_') continue;

    return 0;
  }
  // Minimum name length is 3 chars.
  if (i < 3) { return 0; }
  return 1;
}

int
user_exists(const char *n) {
  char *path;
  struct stat sb;

  if (!valid_user_name(n)) { return 0; }

  asprintf(&path, "%s/%s/conf", bconf_get("user_dir", "users"), n);
  return stat(path, &sb) == 0;
}

User *
user_find(const char *login, const char *password _unused_) {
  if (user_exists(login)) {
    return NULL;
  }

  return NULL;
}

User *
user_guest(const char *login _unused_) {
  return NULL;
}
