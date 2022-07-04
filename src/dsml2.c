#include <cairo-pdf.h>
#include <cjson/cJSON.h>
//#include <curl/curl.h>
#include <lauxlib.h>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <lualib.h>
#include <zlib.h>

#include "dsml2.h"
#include "render.h"
#include "style.h"
#include "traverse.h"
#include "version.h"

/*
 * Generate a checksum value from a file stream.
 */
unsigned int checksumFile(FILE *f) {
  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  rewind(f);

  /*
   * Read the file data into memory
   */
  unsigned char buffer[size + 1];
  buffer[size] = 0;
  int ret = fread(buffer, 1, size, f);
  if (ret != size) {
    fprintf(stderr, "Could not read the expected number of bytes.\n");
    exit(EXIT_FAILURE);
  }
  rewind(f);

  unsigned long crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, buffer, size);

  return crc;
}

cJSON *readJSONFile(FILE *f) {

  /*
   * Get the size of the file so that we know how large we need to make the
   * buffer
   */
  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  rewind(f);

  /*
   * Read the JSON data into memory
   */
  char buffer[size + 1];
  buffer[size] = 0;
  int ret = fread(buffer, 1, size, f);
  if (ret != size) {
    fprintf(stderr, "Could not read the expected number of bytes.\n");
    exit(EXIT_FAILURE);
  }

  /*
   * Parse the data using cJSON
   */
  cJSON *cjson = cJSON_Parse(buffer);
  if (!cjson) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr) {
      fprintf(stderr, "Error before: %s\n", error_ptr);
    }
    cJSON_Delete(cjson);
    exit(EXIT_FAILURE);
  }
  return cjson;
}

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
       * Evaluate a string that sets up some global variables.
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

void usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s [-c content] [-s stylesheet] [-o output file]\n"
          " -c\tThe file that contains the document content. Default \"content.json\".\n"
          " -s\tThe file that contains the document style. Default \"stylesheet.json\".\n"
          " -s\tThe output file. Defaults to stdout.\n"
          "",
          argv[0]);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

  /*
   * The cairo context object.
   */
  cairo_t *cr;

  char outfileName[256] = "/dev/stdout";
  FILE *contentFile = NULL;
  FILE *stylesheetFile = NULL;

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
   * Handle program arguments.
   */
  int opt;
  char *optstring = "c:s:o:h";
  while ((opt = getopt(argc, argv, optstring)) != -1) {
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
  }

  if (optind != argc) {
    fprintf(stderr, "Wrong number of arguments.\n");
    usage(argv);
  }

  /*
   * Failover to default locations if they exist.
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
   * Generate checksums.
   */

  unsigned int contentChecksum = checksumFile(contentFile);
  unsigned int stylesheetChecksum = checksumFile(stylesheetFile);
  fprintf(stdout, "DSML version: %s\n", DSML_VERSION);
  fprintf(stdout, "Content file checksum: %x\n", contentChecksum);
  fprintf(stdout, "Style file checksum: %x\n", stylesheetChecksum);
  setContentChecksum(contentChecksum);
  setStylesheetChecksum(stylesheetChecksum);

  /*
   * Initialize the Cairo surface
   */
  cairo_surface_t *surface = cairo_pdf_surface_create(
      outfileName, 8.5 * POINTS_PER_INCH, 11 * POINTS_PER_INCH);

  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "Invalid filename.\n");
    usage(argv);
  }
  cr = cairo_create(surface);

  cJSON *content = readJSONFile(contentFile);
  cJSON *stylesheet = readJSONFile(stylesheetFile);

  /*
   * Evaluate all constants for use throughout the stylesheet tree
   */
  collectConstants(stylesheet, L);

  simultaneous_traversal(cr, content, stylesheet, L);

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
