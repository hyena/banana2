#include "_page_defs.h"
void bind_pages() {
htserver_bind(htserver, "/notme", mw_session, page_notme);
htserver_bind(htserver, "/foo", mw_postonly, mw_session, page_foo);
htserver_bind(htserver, "/quit", page_quit);
}
