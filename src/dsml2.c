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
 * Print out a brief usage statement and exit
 */
void usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s [-c content] [-s stylesheet] [-o output file]\n"
          " -c,--content      The file that contains the document content. Default \"content.json\".\n"
          " -s,--stylesheet   The file that contains the document style. Default \"stylesheet.json\".\n"
          " -o,--output       The output file. Defaults to stdout.\n"
          " -v,--verbose      Verbose mode.\n"
          " -V,--version      Print out version string.\n"
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
  char *optstring = "c:s:o:hvV";
  static struct option long_options[] = {
      {"content", required_argument, 0, 'c'},
      {"stylesheet", required_argument, 0, 's'},
      {"output", required_argument, 0, 'o'},
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {"version", no_argument, 0, 'V'},
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
    if (opt == 'V') {
      printf("%s\n\n", VERSION_STRING);
      printf("%s\n", LICENSE_STRING);
      exit(EXIT_SUCCESS);
    }
  }

  /*
   * Consume all other arguments.
   */
  if (optind != argc) {
    fprintf(stderr, "Wrong number of arguments.\n");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
    usage(argv);
  }

  /*
   * Failover to default locations if they exist
   */
  if (!contentFile) {
    contentFile = fopen("content.json", "rb");
    if (!contentFile) {
      fprintf(stderr, "Please specify a content file.\n");
      usage(argv);
    }
  }

  if (!stylesheetFile) {
    stylesheetFile = fopen("stylesheet.json", "rb");
    if (!stylesheetFile) {
      fprintf(stderr, "Please specify a stylesheet file.\n");
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
  options options = {0};
  options.pageWidth = 8.5 * POINTS_PER_INCH;
  options.pageHeight = 11 * POINTS_PER_INCH;
  applyOptions(stylesheet, L, &options);

  /*
   * Initialize the Cairo surface
   */
  if (logMode == LOG_VERBOSE) {
    fprintf(stdout, "%f\n", options.pageWidth);
    fprintf(stdout, "%f\n", options.pageHeight);
  }
  cairo_surface_t *surface = cairo_pdf_surface_create(
      outfileName, options.pageWidth, options.pageHeight);

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
