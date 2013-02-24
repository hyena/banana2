/* page_test.c
 *
 */

#include "page.h"
#include "em.h"

PAGE(page_notme, "/notme", mw_session) {
  const char *foo = NULL;
  const char *tpl;
  slog("Notme called");
  foo = htreq_session_get(req);
  if (foo) {
    slog("Notme session found '%s'", foo);
  }
  htreq_strdup(req, HT_TEMPLATE, "foo", "Wallaby Jones");
  tpl = template_eval("foo.html", req);
  slog("template_eval returned '%s'", tpl);

  htreq_send(req, tpl);
}

PAGE(page_foo, "/foo", mw_postonly, mw_session) {
  slog("foo called");
  htreq_session_set(req, "Badonkadonk", NULL);
  htreq_send(req, "Foo! Your session is set!");
}

PAGE(page_quit, "/quit") {
  slog("quit called");
  em_stop();
}

