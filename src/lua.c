#include <cairo-pdf.h>
#include <cjson/cJSON.h>
#include <getopt.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "dsml2.h"
#include "io.h"
#include "lua.h"
#include "render.h"
#include "style.h"
#include "traverse.h"
#include "version.h"

/*
 * Retrieve a floating point value by evaluating a string
 */
float luaGetVal(lua_State *L, char *s) {
  lua_getglobal(L, "eval");
  lua_pushstring(L, s);
  int error = lua_pcall(L, 1, 1, 0);
  float ret = lua_tonumber(L, -1);
  if (error) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    exit(EXIT_FAILURE);
  }
  return ret;
}

/*
 * Set a floating point variable based on a Lua string
 */
void setOption(cJSON *parentElement, lua_State *L, char *str, float *f) {
  cJSON *node = find(parentElement, str);
  if (node) {
    if (cJSON_IsString(node)) {
      *f = luaGetVal(L, node->valuestring);
      return;
    } else if (cJSON_IsNumber(node)) {
      char buf[256];
      snprintf(buf, 255, "%f;", node->valuedouble);
      *f = luaGetVal(L, buf);
      return;
    } else {
      fprintf(stderr, "JSON node unknown format.\n");
      exit(EXIT_FAILURE);
    }
  }
  return;
}

/*
 * Evaluate the "_options" element, which is at the document root and contains
 * page properties such as dimensions and background color
 */
void applyOptions(cJSON *stylesheet, lua_State *L, options *options) {
  cJSON *optionsElement = find(stylesheet, "_options");
  if (optionsElement) {
    setOption(optionsElement, L, "pageWidth", &options->pageWidth);
    setOption(optionsElement, L, "pageHeight", &options->pageHeight);
  }
}

/*
 * Evaluate all of the constants in the "_constants" section for use throughout
 * the document
 */
void collectConstants(cJSON *stylesheet, lua_State *L) {
  cJSON *styleElement = find(stylesheet, "_constants");
  if (styleElement) {
    cJSON *node = NULL;

    node = styleElement->child;
    while (1) {
      if (!node) {
        break;
      }

      /*
       * Evaluate a string that sets up some global variables
       */
      char buf[256];
      if (cJSON_IsString(node)) {
        snprintf(buf, 255, "%s = %s;", node->string, node->valuestring);
      } else if (cJSON_IsNumber(node)) {
        snprintf(buf, 255, "%s = %f;", node->string, node->valuedouble);
      } else {
        fprintf(stderr, "JSON node unknown format.\n");
        exit(EXIT_FAILURE);
      }
      int error = luaL_loadbuffer(L, buf, strlen(buf), "") || lua_pcall(L, 0, 0, 0);
      if (error) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        exit(EXIT_FAILURE);
      }

      node = node->next;
    }
  }
}
