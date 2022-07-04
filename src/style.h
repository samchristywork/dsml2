#ifndef STYLE_H
#define STYLE_H

#include <lauxlib.h>
#include <lualib.h>

enum align {
  ALIGN_LEFT = 0,
  ALIGN_CENTER = 1,
  ALIGN_RIGHT = 2,
};

/*
 * Struct that holds information about the various styles that need to be
 * applied and propagated to children.
 */
typedef struct style {
  float x;
  float y;
  float xOffset;
  float yOffset;
  float size;
  float textWidth;
  float lineHeight;
  float spacing;
  float r;
  float g;
  float b;
  float a;
  float width;
  int stripNewlines;
  int textAlign;
  char face[256];
  char uri[256];
} style;

void applyStyles(cairo_t *cr, cJSON *styleElement, struct style *style, lua_State *L);

#endif
