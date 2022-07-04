#ifndef TRAVERSE_H
#define TRAVERSE_H

cJSON *find(cJSON *tree, char *str);
void simultaneous_traversal(cairo_t *cr, cJSON *content, cJSON *stylesheet);

#endif
