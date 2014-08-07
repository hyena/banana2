/* page_test.c
 *
 */

#include "page.h"
#include "lib/em.h"

typedef struct user User;

MIDDLEWARE(mw_nouser) {
  User *user = (User *) htreq_session_get(req);
  if (user) {
    htreq_t_send(req, "foo.html");
  } else {
    htreq_next(req);
  }
}

MIDDLEWARE(mw_useronly) {
  User *user = (User *) htreq_session_get(req);
  if (user) {
    htreq_not_allowed(req);
  } else {
    htreq_next(req);
  }
}

PAGE(page_notme, "/login", mw_session) {
  htreq_t_send(req, "login.html");
}

PAGE(page_login, "/action/login", mw_postonly, mw_session, mw_nouser) {
}
