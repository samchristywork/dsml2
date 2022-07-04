#include <cjson/cJSON.h>
#include <pango/pangocairo.h>

#include "render.h"

/*
 * Various checksums and their `set` functions.
 */
unsigned int contentChecksum;
unsigned int stylesheetChecksum;

void setContentChecksum(unsigned int checksum) {
  contentChecksum = checksum;
}

void setStylesheetChecksum(unsigned int checksum) {
  stylesheetChecksum = checksum;
}

void renderText(cairo_t *cr, cJSON *content, struct style *style) {
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
     * Write out the current date and time.
     */
    if (strcmp(content->valuestring, "CURRENT_DATE") == 0) {
      time_t now;
      time(&now);
      pango_layout_set_markup(layout, ctime(&now), -1);

      /*
     * Transclusion directive. The token "INCLUDE:" will indicate that the text
     * after the colon is a filename, the contents of which will be treated as
     * the text of the current element.
     */
    } else if (strncmp(content->valuestring, "INCLUDE:", strlen("INCLUDE:")) == 0) {
      for (int i = 0; i < strlen(content->valuestring); i++) {
        if (content->valuestring[i] == ':') {
          FILE *f = fopen(content->valuestring + i + 1, "rb");
          if (!f) {
            fprintf(stderr, "Trying to open \"%s\".\n", content->valuestring + i + 1);
            perror("fopen");
            exit(EXIT_FAILURE);
          }

          fseek(f, 0, SEEK_END);
          int size = ftell(f);
          rewind(f);

          /*
           * Read the file data into memory
           */
          char buffer[size + 1];
          buffer[size] = 0;
          int ret = fread(buffer, 1, size, f);
          if (ret != size) {
            fprintf(stderr, "Could not read the expected number of bytes.\n");
            exit(EXIT_FAILURE);
          }
          rewind(f);

          if (style->stripNewlines) {
            for (int i = 0; i < size; i++) {
              if (buffer[i] == '\n') {
                buffer[i] = ' ';
              }
            }
          }

          pango_layout_set_markup(layout, buffer, -1);
        }
      }

      /*
     * Replace the text of the current element with the DSML version and
     * checksums required to build the document.
     */
    } else if (strcmp(content->valuestring, "REV") == 0) {
      char buf[256];
      snprintf(buf, 255, "%s:%x:%x", DSML_VERSION, contentChecksum, stylesheetChecksum);
      pango_layout_set_markup(layout, buf, -1);

      /*
     * Reproduce the text verbatim.
     */
    } else {
      pango_layout_set_markup(layout, content->valuestring, -1);
    }

    /*
     * Render the text
     */
    if (style->uri) {
      cairo_tag_begin(cr, CAIRO_TAG_LINK, style->uri);
    }
    cairo_move_to(cr, style->x, style->y);
    pango_cairo_show_layout(cr, layout);
    if (style->uri) {
      cairo_tag_end(cr, CAIRO_TAG_LINK);
    }

    /*
     * Cleanup
     */
    g_object_unref(layout);
    pango_font_description_free(font_description);
  }
}
