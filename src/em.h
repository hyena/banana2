/* em.h
 *
 * Global EventMachine handler
 */

#ifndef __MY_EM_H_
#define __MY_EM_H_

#include <event2/event.h>

extern struct event_base *eventmachine;

struct event_base *em_init();
void em_start();
void em_stop();

typedef void (*timerhandler)(void *ptr);
#define TIMER(x) void x(void *ptr)

void em_loop(const char *name, int seconds, timerhandler handler, void *ptr);

#endif /* __MY_EM_H_ */
