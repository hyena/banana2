/* banana.h
 */

#ifndef _MY_BANANA_H_
#define _MY_BANANA_H_

#include "logger.h"
// slog() defined in logger.h
#include "htserver.h"
extern struct htserver *htserver;

#include "config.h"
extern struct config *bconfig;
#define bconf_get(name,def) conf_get(bconfig, name, def)
#define bconf_int(name,def) conf_int(bconfig, name, def)
#define bconf_is(name,def)  conf_is(bconfig, name, def)

#define _unused_ __attribute__ ((__unused__))

#endif
