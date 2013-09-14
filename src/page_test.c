/* page_test.c
 *
 */

#include "page.h"
#include "lib/em.h"

PAGE(page_notme, "/notme", mw_session) {
  const char *foo = NULL;
  slog("Notme called");
  foo = htreq_session_get(req);
  if (foo) {
    slog("Notme session found '%s'", foo);
  }
  htreq_t_strdup(req, "foo", "Wallaby Jones");
  htreq_t_send(req, "foo.html");
}

PAGE(page_foo, "/foo", mw_postonly, mw_session) {
  slog("foo called");
  htreq_session_set(req, "Badonkadonk", NULL);
  htreq_send(req, CTYPE_TEXT, "Foo! Your session is set!");
}

PAGE(page_quit, "/quit") {
  slog("quit called");
  em_stop();
}

