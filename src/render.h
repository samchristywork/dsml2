#ifndef RENDER_H
#define RENDER_H

#include "style.h"

void setContentChecksum(unsigned int checksum);
void setStylesheetChecksum(unsigned int checksum);
void renderText(cairo_t *cr, cJSON *content, style *style);
void handleIcons(cairo_t *cr, cJSON *stylesheet, style *style);

#endif
