/* user.h
 */

#ifndef _MY_USER_H_
#define _MY_USER_H_

#include "banana.h"

typedef struct user User;

User* user_find(const char *login, const char *password);
User* user_guest(const char *name);

#endif
