/* config.h
 *
 * Read a config file with ^key:\s*(val)\s*$ lines.
 */

#ifndef _MY_CONFIG_H
#define _MY_CONFIG_H

/* Maximum length of the name and the value. */
#define CONF_MAX_LEN 30

struct config;

struct config *conf_read(const char *pathname);
struct config *conf_free(struct config *tofree);

/* Returns char * value - Don't free it. */
const char *conf_get(struct config *conf, const char *name, const char *def);
/* Like conf_get, but parses into integer. */
int conf_int(struct config *conf, const char *name, int def);
/* Like conf_get, but returns 1 if value is, ignoring case, one of:
 * 'y', 'yes', '1', 'on', 'true' or 't'
 * */
int conf_is(struct config *conf, const char *name, int def);

#endif /* _MY_CONFIG_H */
