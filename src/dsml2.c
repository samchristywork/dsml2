#include <cairo-pdf.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <lauxlib.h>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <lualib.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#define DSML_VERSION "2.0.0"

/*
 * Points are the unit of measurement used by Cairo. 1 point is equal to 1/72
 * inches.
 */
#define POINTS_PER_INCH 72

/*
 * Struct that holds information about the various styles that need to be
 * applied and propagated to children.
 */
struct style {
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
  int textAlign;
  char face[256];
  char link[256];
};

enum align {
  ALIGN_LEFT = 0,
  ALIGN_CENTER = 1,
  ALIGN_RIGHT = 2,
};

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
 * The Cairo context object.
 */
cairo_t *cr;

/*
 * The Lua state object.
 */
lua_State *L;

unsigned int contentChecksum;
unsigned int stylesheetChecksum;

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
 * Checks a node and all of its siblings for a particular key string.
 */
cJSON *find(cJSON *tree, char *str) {
  cJSON *node = NULL;

  if (tree) {
    node = tree->child;
    while (1) {
      if (!node) {
        break;
      }
      if (strcmp(str, node->string) == 0) {
        break;
      }
      node = node->next;
    }
  }
  return node;
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

void applyStyles(cJSON *styleElement, struct style *style) {
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
    cJSON *face = find(styleElement, "face");
    if (face) {
      strcpy(style->face, face->valuestring);
    }
    cJSON *link = find(styleElement, "link");
    if (link) {
      strcpy(style->link, link->valuestring);
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

void handleIcons(cJSON *stylesheet, struct style *style) {

  /*
   * This section of code is run whenever the "icon" element is encountered
   */
  cJSON *icon = find(stylesheet, "icon");
  if (icon) {

    /*
     * The "icon" element should have at least a URL and filename nested within
     * it.
     */
    cJSON *u = find(icon, "url");
    cJSON *n = find(icon, "name");
    if (u && n) {

      /*
       * Save the context and apply transformations
       */
      cairo_save(cr);
      cairo_translate(cr, style->x, style->y);
      cairo_scale(cr, style->size, style->size);

      /*
       * Try to open the image, download it from the internet if that doesn't
       * succeed
       */
      RsvgHandle *rsvg = rsvg_handle_new_from_file(n->valuestring, 0);
      if (!rsvg) {
        fprintf(stderr, "File was not found locally, downloading.\n");
        CURL *handle = curl_easy_init();
        if (handle) {
          FILE *f = fopen(n->valuestring, "wb");
          curl_easy_setopt(handle, CURLOPT_URL, u->valuestring);
          curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
          curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
          curl_easy_perform(handle);
          curl_easy_cleanup(handle);
          fclose(f);
        } else {
          fprintf(stderr, "cURL failure.\n");
          exit(EXIT_FAILURE);
        }
        rsvg = rsvg_handle_new_from_file(n->valuestring, 0);
      }

      /*
       * Display the image
       */
      if (!rsvg_handle_render_cairo(rsvg, cr)) {
        fprintf(stderr, "Icon could not be rendered.\n");
        exit(EXIT_FAILURE);
      }

      /*
       * Restore the graphics context
       */
      cairo_restore(cr);
    }
  }
}

void renderText(cJSON *content, struct style *style) {
  if (cJSON_IsString(content) && content->valuestring) {

    /*
     * Configure the style of text that is to be displayed
     */
    cairo_set_source_rgba(cr, style->r, style->g, style->b, style->a);
    PangoFontDescription *font_description = pango_font_description_new();
    pango_font_description_set_family(font_description, style->face);
    pango_font_description_set_absolute_size(font_description,
                                             style->size * PANGO_SCALE);

    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, font_description);

    pango_layout_set_justify(layout, TRUE);

    pango_layout_set_line_spacing(layout, style->spacing);

    if (style->width != 0) {
      pango_layout_set_width(layout, PANGO_SCALE * style->width);
    }
    if (style->textAlign == ALIGN_CENTER) {
      pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
      if (style->width == 0) {
        fprintf(stderr, "Alignment set to 'center', but missing width.\n");
      }
      style->x -= style->width / 2.;
    } else if (style->textAlign == ALIGN_RIGHT) {
      pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
      if (style->width == 0) {
        fprintf(stderr, "Alignment set to 'right', but missing width.\n");
      }
      style->x -= style->width;
    }

    /*
     * Render the text
     */
    if (strcmp(content->valuestring, "CURRENT_DATE") == 0) {
      time_t now;
      time(&now);
      pango_layout_set_markup(layout, ctime(&now), -1);
    } else if (strcmp(content->valuestring, "REV") == 0) {
      char buf[256];
      snprintf(buf, 255, "%s:%x:%x", DSML_VERSION, contentChecksum, stylesheetChecksum);
      pango_layout_set_markup(layout, buf, -1);
    } else {
      pango_layout_set_markup(layout, content->valuestring, -1);
    }
    cairo_move_to(cr, style->x, style->y);
    pango_cairo_show_layout(cr, layout);

    /*
     * Cleanup
     */
    g_object_unref(layout);
    pango_font_description_free(font_description);
  }
}

/*
 * This function traverses the content and stylesheet trees simultaneously and
 * applies style information and draws elements along the way.
 */
void _simultaneous_traversal(cJSON *content, cJSON *stylesheet, int depth,
                             struct style style) {

  cJSON *styleElement = find(stylesheet, "_style");
  applyStyles(styleElement, &style);

  handleIcons(stylesheet, &style);

  renderText(content, &style);

  cJSON *contentNode = content->child;

  /*
   * Traverse the children
   */
  while (1) {
    if (!contentNode) {
      break;
    }
    fprintf(stdout, "Processing node: ");
    for (int i = 0; i < depth; i++) {
      fprintf(stdout, "  ");
    }
    fprintf(stdout, "%s\n", contentNode->string);

    /*
     * Find the correct node in the stylesheet to follow along
     */
    cJSON *styleNode = find(stylesheet, contentNode->string);

    /*
     * Recur
     */
    _simultaneous_traversal(contentNode, styleNode, depth + 1, style);

    cJSON *styleElement = find(stylesheet, "_style");
    if (styleElement) {
      cJSON *x = find(styleElement, "xOffset");
      if (x) {
        style.x += x->valuedouble;
      }
      cJSON *y = find(styleElement, "yOffset");
      if (y) {
        style.y += y->valuedouble;
      }
    }

    contentNode = contentNode->next;
  }
}

void simultaneous_traversal(cJSON *content, cJSON *stylesheet) {
  /*
   * Apply default styling rules.
   */
  struct style style = {0};
  style.size = 12;
  style.a = 1;
  style.lineHeight = 1.5;
  strcpy(style.face, "Sans");
  _simultaneous_traversal(content, stylesheet, 0, style);
}

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

int main(int argc, char *argv[]) {

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

  contentChecksum = checksumFile(contentFile);
  stylesheetChecksum = checksumFile(stylesheetFile);
  fprintf(stdout, "DSML version: %s\n", DSML_VERSION);
  fprintf(stdout, "Content file checksum: %x\n", contentChecksum);
  fprintf(stdout, "Style file checksum: %x\n", stylesheetChecksum);

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

  simultaneous_traversal(content, stylesheet);

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
