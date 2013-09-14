// template.h
//
// Banana's template engine
//
#ifndef _MY_TEMPLATE_H_
#define _MY_TEMPLATE_H_

#include "lib/htserver.h"

// template_find gets full path to a template using lists of mempools.
const char *template_eval(const char *filename, struct htreq *req);
void template_cleanup();

// templatevars
extern struct config *templatevars;

#endif /* _MY_EVHTTPSERVER_H_ */
