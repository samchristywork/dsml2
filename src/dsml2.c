#include <cairo-pdf.h>
#include <cjson/cJSON.h>
//#include <curl/curl.h>
#include <lauxlib.h>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <lualib.h>
#include <zlib.h>

#include "dsml2.h"
#include "render.h"
#include "traverse.h"

/*
 * Macros for applying style information.
 */
#define ADD_STYLE_DOUBLE(e, a, b) \
  {                               \
    cJSON *s = find(e, a);        \
    if (s) {                      \
      luaEval(s);                 \
      b += s->valuedouble;        \
    }                             \
  }

#define APPLY_STYLE_DOUBLE(e, a, b) \
  {                                 \
    cJSON *s = find(e, a);          \
    if (s) {                        \
      luaEval(s);                   \
      b = s->valuedouble;           \
    }                               \
  }

/*
 * The callback that cURL uses to write the icon file to the filesystem.
 */
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return fwrite(ptr, size, nmemb, stream);
}

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

/*
 * Evaluate an arithmetic expression in the `valuestring` field of the cJSON
 * struct, and place the floating point contents into the `valuedouble` field.
 * Cause program exit on invalid input.
 */
void luaEval(cJSON *c) {
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

void applyStyles(cairo_t *cr, cJSON *styleElement, struct style *style) {
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

//void handleIcons(cairo_t *cr, cJSON *stylesheet, struct style *style) {
//
//  /*
//   * This section of code is run whenever the "icon" element is encountered
//   */
//  cJSON *icon = find(stylesheet, "icon");
//  if (icon) {
//
//    /*
//     * The "icon" element should have at least a URL and filename nested within
//     * it.
//     */
//    cJSON *u = find(icon, "url");
//    cJSON *n = find(icon, "name");
//    if (u && n) {
//
//      /*
//       * Save the context and apply transformations
//       */
//      cairo_save(cr);
//      cairo_translate(cr, style->x, style->y);
//      cairo_scale(cr, style->size, style->size);
//
//      /*
//       * Try to open the image, download it from the internet if that doesn't
//       * succeed
//       */
//      RsvgHandle *rsvg = rsvg_handle_new_from_file(n->valuestring, 0);
//      if (!rsvg) {
//        fprintf(stderr, "File was not found locally, downloading.\n");
//        CURL *handle = curl_easy_init();
//        if (handle) {
//          FILE *f = fopen(n->valuestring, "wb");
//          curl_easy_setopt(handle, CURLOPT_URL, u->valuestring);
//          curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
//          curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
//          curl_easy_perform(handle);
//          curl_easy_cleanup(handle);
//          fclose(f);
//        } else {
//          fprintf(stderr, "cURL failure.\n");
//          exit(EXIT_FAILURE);
//        }
//        rsvg = rsvg_handle_new_from_file(n->valuestring, 0);
//      }
//
//      /*
//       * Display the image
//       */
//      if (!rsvg_handle_render_cairo(rsvg, cr)) {
//        fprintf(stderr, "Icon could not be rendered.\n");
//        exit(EXIT_FAILURE);
//      }
//
//      /*
//       * Restore the graphics context
//       */
//      cairo_restore(cr);
//    }
//  }
//}

void collectConstants(cJSON *stylesheet) {
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
  L = luaL_newstate();
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
  collectConstants(stylesheet);

  simultaneous_traversal(cr, content, stylesheet);

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
