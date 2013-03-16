// template.h
//
// Banana's template engine
//
#ifndef _MY_TEMPLATE_H_
#define _MY_TEMPLATE_H_

#include "htserver.h"

// template_find gets full path to a template using req
// Since user templates override global templates.
const char *template_find(const char *filename, struct htreq *req);
const char *template_eval(const char *filename, struct htreq *req);
void template_cleanup();

// templatevars
extern struct config *templatevars;

#endif /* _MY_EVHTTPSERVER_H_ */
