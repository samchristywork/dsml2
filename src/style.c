#include <cairo-pdf.h>
#include <cjson/cJSON.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "dsml2.h"
#include "render.h"
#include "style.h"
#include "traverse.h"
#include "version.h"

/*
 * Macros for applying style information.
 */
#define ADD_STYLE_DOUBLE(e, a, b) \
  {                               \
    cJSON *s = find(e, a);        \
    if (s) {                      \
      luaEval(s, L);              \
      b += s->valuedouble;        \
    }                             \
  }

#define APPLY_STYLE_DOUBLE(e, a, b) \
  {                                 \
    cJSON *s = find(e, a);          \
    if (s) {                        \
      luaEval(s, L);                \
      b = s->valuedouble;           \
    }                               \
  }

/*
 * Evaluate an arithmetic expression in the `valuestring` field of the cJSON
 * struct, and place the floating point contents into the `valuedouble` field.
 * Cause program exit on invalid input.
 */
void luaEval(cJSON *c, lua_State *L) {
  if (cJSON_IsString(c)) {
    lua_getglobal(L, "eval");
    lua_pushstring(L, c->valuestring);
    int error = lua_pcall(L, 1, 1, 0);
    c->valuedouble = lua_tonumber(L, -1);
    if (error) {
      fprintf(stderr, "%s\n", lua_tostring(L, -1));
      exit(EXIT_FAILURE);
    }
  }
}

void applyStyles(cairo_t *cr, cJSON *styleElement, struct style *style, lua_State *L) {
  if (styleElement) {
    ADD_STYLE_DOUBLE(styleElement, "x", style->x)
    ADD_STYLE_DOUBLE(styleElement, "y", style->y)
    APPLY_STYLE_DOUBLE(styleElement, "r", style->r)
    APPLY_STYLE_DOUBLE(styleElement, "g", style->g)
    APPLY_STYLE_DOUBLE(styleElement, "b", style->b)
    APPLY_STYLE_DOUBLE(styleElement, "a", style->a)
    APPLY_STYLE_DOUBLE(styleElement, "size", style->size)
    APPLY_STYLE_DOUBLE(styleElement, "spacing", style->spacing)
    APPLY_STYLE_DOUBLE(styleElement, "width", style->width)
    APPLY_STYLE_DOUBLE(styleElement, "textWidth", style->textWidth)
    APPLY_STYLE_DOUBLE(styleElement, "lineHeight", style->lineHeight)
    cJSON *stripNewlines = find(styleElement, "stripNewlines");
    if (stripNewlines) {
      if (cJSON_IsBool(stripNewlines)) {
        if (cJSON_IsTrue(stripNewlines)) {
          style->stripNewlines = 1;
        }
      }
    }
    cJSON *face = find(styleElement, "face");
    if (face) {
      strcpy(style->face, face->valuestring);
    }
    cJSON *uri = find(styleElement, "URI");
    if (uri) {
      strcpy(style->uri, uri->valuestring);
    }
    cJSON *textAlign = find(styleElement, "textAlign");
    if (textAlign) {
      if (strcmp("center", textAlign->valuestring) == 0) {
        style->textAlign = ALIGN_CENTER;
      } else if (strcmp("left", textAlign->valuestring) == 0) {
        style->textAlign = ALIGN_LEFT;
      } else if (strcmp("right", textAlign->valuestring) == 0) {
        style->textAlign = ALIGN_RIGHT;
      }
    }
    cJSON *contentNode = styleElement->child;
    while (1) {
      if (!contentNode) {
        break;
      }
      if (strcmp("line", contentNode->string) == 0) {
        double x1, x2, y1, y2;
        double r = 0;
        double g = 0;
        double b = 0;
        double a = 1;
        double lineWidth = 1;
        APPLY_STYLE_DOUBLE(contentNode, "x1", x1);
        APPLY_STYLE_DOUBLE(contentNode, "x2", x2);
        APPLY_STYLE_DOUBLE(contentNode, "y1", y1);
        APPLY_STYLE_DOUBLE(contentNode, "y2", y2);
        APPLY_STYLE_DOUBLE(contentNode, "width", lineWidth);
        APPLY_STYLE_DOUBLE(contentNode, "r", r);
        APPLY_STYLE_DOUBLE(contentNode, "g", g);
        APPLY_STYLE_DOUBLE(contentNode, "b", b);
        APPLY_STYLE_DOUBLE(contentNode, "a", a);

        x1 += style->x;
        x2 += style->x;
        y1 += style->y;
        y2 += style->y;

        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_set_line_width(cr, lineWidth);
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
      }
      contentNode = contentNode->next;
    }
  }
}
