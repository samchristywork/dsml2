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
#include "render.h"
#include "style.h"
#include "traverse.h"
#include "version.h"

float pageWidth = 8.5 * POINTS_PER_INCH;
float pageHeight = 11 * POINTS_PER_INCH;

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
void applyOptions(cJSON *stylesheet, lua_State *L) {
  cJSON *optionsElement = find(stylesheet, "_options");
  if (optionsElement) {
    setOption(optionsElement, L, "pageWidth", &pageWidth);
    setOption(optionsElement, L, "pageHeight", &pageHeight);
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

/*
 * Print out a brief usage statement and exit
 */
void usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s [-c content] [-s stylesheet] [-o output file]\n"
          " -c,--content      The file that contains the document content. Default \"content.json\".\n"
          " -s,--stylesheet   The file that contains the document style. Default \"stylesheet.json\".\n"
          " -o,--output       The output file. Defaults to stdout.\n"
          " -v,--verbose      Verbose mode.\n"
          "",
          argv[0]);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

  /*
   * The cairo context object
   */
  cairo_t *cr;

  char outfileName[256] = "/dev/stdout";
  FILE *contentFile = NULL;
  FILE *stylesheetFile = NULL;
  int logMode = LOG_NONE;

  /*
   * Initialize the Lua library
   */

  lua_State *L = luaL_newstate();
  luaL_openlibs(L);

  /*
   * Evaluate a string that defines a function
   */
  char *buf = "function eval(n);return load('return '..n)();end";
  int error = luaL_loadbuffer(L, buf, strlen(buf), "") || lua_pcall(L, 0, 0, 0);
  if (error) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    exit(EXIT_FAILURE);
  }

  /*
   * Handle program arguments
   */
  int opt;
  int option_index = 0;
  char *optstring = "c:s:o:hv";
  static struct option long_options[] = {
      {"content", required_argument, 0, 'c'},
      {"stylesheet", required_argument, 0, 's'},
      {"output", required_argument, 0, 'o'},
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {0, 0, 0, 0},
  };
  while ((opt = getopt_long(argc, argv, optstring, long_options, &option_index)) != -1) {
    if (opt == 'h') {
      usage(argv);
    }
    if (opt == 'o') {
      strncpy(outfileName, optarg, 255);
    }
    if (opt == 'c') {
      contentFile = fopen(optarg, "rb");
      if (!contentFile) {
        perror("fopen");
        usage(argv);
      }
    }
    if (opt == 's') {
      stylesheetFile = fopen(optarg, "rb");
      if (!stylesheetFile) {
        perror("fopen");
        usage(argv);
      }
    }
    if (opt == 'v') {
      logMode = LOG_VERBOSE;
    }
  }

  if (optind != argc) {
    fprintf(stderr, "Wrong number of arguments.\n");
    usage(argv);
  }

  /*
   * Failover to default locations if they exist
   */
  if (!contentFile) {
    contentFile = fopen("content.json", "rb");
    if (!contentFile) {
      perror("fopen");
      usage(argv);
    }
  }

  if (!stylesheetFile) {
    stylesheetFile = fopen("stylesheet.json", "rb");
    if (!stylesheetFile) {
      perror("fopen");
      usage(argv);
    }
  }

  /*
   * Generate checksums
   */

  unsigned int contentChecksum = checksumFile(contentFile);
  unsigned int stylesheetChecksum = checksumFile(stylesheetFile);
  if (logMode == LOG_VERBOSE) {
    fprintf(stdout, "DSML version: %s\n", DSML_VERSION);
    fprintf(stdout, "Content file checksum: %x\n", contentChecksum);
    fprintf(stdout, "Style file checksum: %x\n", stylesheetChecksum);
  }
  setContentChecksum(contentChecksum);
  setStylesheetChecksum(stylesheetChecksum);

  /*
   * Ingest files
   */
  cJSON *content = readJSONFile(contentFile);
  cJSON *stylesheet = readJSONFile(stylesheetFile);

  /*
   * Evaluate all constants for use throughout the stylesheet tree
   */
  collectConstants(stylesheet, L);

  /*
   * Find all page properties in the "_options" element and apply them
   */
  applyOptions(stylesheet, L);

  /*
   * Initialize the Cairo surface
   */
  if (logMode == LOG_VERBOSE) {
    fprintf(stdout, "%f\n", pageWidth);
    fprintf(stdout, "%f\n", pageHeight);
  }
  cairo_surface_t *surface = cairo_pdf_surface_create(
      outfileName, pageWidth, pageHeight);

  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "Invalid filename.\n");
    usage(argv);
  }
  cr = cairo_create(surface);

  simultaneous_traversal(cr, content, stylesheet, L, logMode);

  /*
   * Technically, we don't need to use this function because we are only
   * rendering a single page. If we were rendering multiple pages then we would
   * use multiple calls to cairo_show_page().
   */
  cairo_show_page(cr);

  /*
   * Cleanup
   */
  cairo_surface_destroy(surface);
  cairo_destroy(cr);

  fclose(contentFile);
  fclose(stylesheetFile);
  cJSON_Delete(content);
  cJSON_Delete(stylesheet);

  lua_close(L);
}
