#include <cairo-pdf.h>
#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <librsvg-2.0/librsvg/rsvg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
  int x;
  int y;
  int xoffset;
  int yoffset;
  float size;
  float r;
  float g;
  float b;
  float a;
  char face[256];
};

/*
 * Macros for applying style information.
 */
#define ADD_STYLE_INT(a, b) { cJSON *s = find(pos, a); \
  if(s){ b += s->valueint; } }

#define APPLY_STYLE_INT(a, b) { cJSON *s = find(pos, a); \
  if(s){ b = s->valueint; } }

#define APPLY_STYLE_DOUBLE(a, b) { cJSON *s = find(pos, a); \
  if(s){ b = s->valuedouble; } }

/*
 * The Cairo context object.
 */
cairo_t *cr;

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

cJSON *readJSONFile(FILE *f){

  /*
   * Get the size of the file so that we know how large we need to make the
   * buffer
   */
  fseek(f, 0, SEEK_END);
  int size=ftell(f);
  rewind(f);

  /*
   * Read the JSON data into memory
   */
  char buffer[size+1];
  buffer[size]=0;
  int ret=fread(buffer, 1, size, f);
  if(ret!=size){
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
 * Traverse an entire JSON tree, printing out nodes with proper indentation
 * along the way.
 */
void _traverse(cJSON *cjson, int depth) {
  cJSON *node = cjson;
  while (1) {
    if (!node) {
      break;
    }
    for (int i = 0; i < depth; i++) {
      printf(" ");
    }
    puts(node->string);
    if (node->child) {
      _traverse(node->child, depth + 1);
    }
    node = node->next;
  }
}

void traverse(cJSON *cjson) { _traverse(cjson, 0); }

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
 * This function traverses the content and stylesheet trees simultaneously and
 * applies style information and draws elements along the way.
 */
void _simultaneous_traversal(cJSON *content, cJSON *stylesheet, int depth,
                             struct style style) {

  cJSON *pos = find(stylesheet, "position");
  if (pos) {
    ADD_STYLE_INT("x", style.x)
    ADD_STYLE_INT("y", style.y)
    APPLY_STYLE_DOUBLE("r", style.r)
    APPLY_STYLE_DOUBLE("g", style.g)
    APPLY_STYLE_DOUBLE("b", style.b)
    APPLY_STYLE_DOUBLE("a", style.a)
    APPLY_STYLE_DOUBLE("size", style.size)
    cJSON *face = find(pos, "face");
    if (face) {
      strcpy(style.face, face->valuestring);
    }
  }

  cJSON *icon = find(stylesheet, "icon");
  if (icon) {
    cJSON *u = find(icon, "url");
    cJSON *n = find(icon, "name");

    if(u && n){
      cairo_save(cr);
      cairo_translate(cr, style.x, style.y);
      cairo_scale(cr, style.size, style.size);
      RsvgHandle *rsvg = rsvg_handle_new_from_file(n->valuestring, 0);

      if(!rsvg){
        printf("File was not found locally, downloading.\n");
        CURL *handle = curl_easy_init();
        if (handle) {
          FILE *f = fopen(n->valuestring, "wb");
          curl_easy_setopt(handle, CURLOPT_URL, u->valuestring);
          curl_easy_setopt(handle, CURLOPT_WRITEDATA, f);
          curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
          CURLcode res = curl_easy_perform(handle);
          curl_easy_cleanup(handle);
          fclose(f);
        }else{
          fprintf(stderr, "cURL failure.\n");
          exit(EXIT_FAILURE);
        }
        rsvg = rsvg_handle_new_from_file(n->valuestring, 0);
      }

      if(!rsvg_handle_render_cairo(rsvg, cr)){
        fprintf(stderr, "Icon could not be rendered.\n");
        exit(EXIT_FAILURE);
      }
      cairo_restore(cr);
    }
  }

  if (cJSON_IsString(content) && content->valuestring) {

    /*
     * Configure the style of text that is to be displayed
     */
    cairo_set_source_rgba(cr, style.r, style.g, style.b, style.a);
    cairo_set_font_size(cr, style.size);
    cairo_select_font_face(cr, style.face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);

    /*
     * Render the text
     */
    cairo_move_to(cr, style.x, style.y);
    cairo_show_text(cr, content->valuestring);
  }

  cJSON *contentNode = content->child;

  /*
   * Traverse the children
   */
  while (1) {
    if (!contentNode) {
      break;
    }

    /*
     * Find the correct node in the stylesheet to follow along
     */
    cJSON *styleNode = find(stylesheet, contentNode->string);

    /*
     * Recur
     */
    _simultaneous_traversal(contentNode, styleNode, depth + 1, style);

    cJSON *pos = find(stylesheet, "position");
    if (pos) {
      cJSON *x = find(pos, "xoffset");
      if (x) {
        style.x += x->valueint;
      }
      cJSON *y = find(pos, "yoffset");
      if (y) {
        style.y += y->valueint;
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
  style.size = 10;
  style.a = 1;
  strcpy(style.face, "Sans");
  _simultaneous_traversal(content, stylesheet, 0, style);
}

int main(int argc, char *argv[]) {

  char outfileName[256] = "/dev/stdout";
  FILE *contentFile = NULL;
  FILE *stylesheetFile = NULL;

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
   * Initialize the Cairo surface
   */
  cairo_surface_t *surface = cairo_pdf_surface_create(
      outfileName, 8.5 * POINTS_PER_INCH, 11 * POINTS_PER_INCH);

  if(cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS){
    fprintf(stderr, "Invalid filename.\n");
    usage(argv);
  }
  cr = cairo_create(surface);

  cJSON *content = readJSONFile(contentFile);
  cJSON *stylesheet = readJSONFile(stylesheetFile);

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
}
