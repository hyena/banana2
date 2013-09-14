/* banana.h
 */

#ifndef _MY_BANANA_H_
#define _MY_BANANA_H_

#include "lib/logger.h"
// slog() defined in logger.h
#include "lib/htserver.h"
extern struct htserver *htserver;

#include "lib/config.h"
#include "bconfig.h"

#define _unused_ __attribute__ ((__unused__))

#endif
