#ifndef RENDER_H
#define RENDER_H

#include "style.h"
#include "version.h"

void setContentChecksum(unsigned int checksum);
void setStylesheetChecksum(unsigned int checksum);
void renderText(cairo_t *cr, cJSON *content, struct style *style);

#endif
