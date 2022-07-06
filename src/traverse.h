#ifndef TRAVERSE_H
#define TRAVERSE_H

enum { LOG_NONE = 0,
       LOG_VERBOSE = 1 };

cJSON *find(cJSON *tree, char *str);
void simultaneous_traversal(cairo_t *cr, cJSON *content, cJSON *stylesheet, lua_State *L, int logMode);

#endif
