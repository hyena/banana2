/* bconfig.h
 */

#ifndef _MY_BANANACONF_H_
#define _MY_BANANACONF_H_

#include "config.h"
extern struct config *bconfig;
#define bconf_get(name,def) conf_get(bconfig, name, def)
#define bconf_int(name,def) conf_int(bconfig, name, def)
#define bconf_is(name,def)  conf_is(bconfig, name, def)

// HTTP config options
#define LISTEN_HOST      bconf_get("listen_host", "0.0.0.0")
#define LISTEN_PORT      bconf_int("listen_port", 4080)
#define HTTP_SIGNATURE   bconf_get("http_signature", "Banana MUSH Client")
#define HTTP_ROOT        bconf_get("file_root", "public")

// Print warnings when long poll lasts longer than this.
#define LONGPOLL_TIMEOUT bconf_int("longpoll_timeout", 120)
#define SESSION_TIMEOUT  (LONGPOLL_TIMEOUT + 20)

#define _unused_ __attribute__ ((__unused__))

#endif
