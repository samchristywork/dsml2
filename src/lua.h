#ifndef LUA_H
#define LUA_H

typedef struct options {
  float pageWidth;
  float pageHeight;
} options;

float luaGetVal(lua_State *L, char *s);
void setOption(cJSON *parentElement, lua_State *L, char *str, float *f);
void applyOptions(cJSON *stylesheet, lua_State *L, options *options);
void collectConstants(cJSON *stylesheet, lua_State *L);

#endif
