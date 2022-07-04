#include <cjson/cJSON.h>
#include <librsvg-2.0/librsvg/rsvg.h>

#include "render.h"
#include "style.h"

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
void _simultaneous_traversal(cairo_t *cr, cJSON *content, cJSON *stylesheet, int depth,
                             struct style style, lua_State *L) {

  cJSON *styleElement = find(stylesheet, "_style");
  applyStyles(cr, styleElement, &style, L);

  handleIcons(cr, stylesheet, &style);

  renderText(cr, content, &style);

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
    _simultaneous_traversal(cr, contentNode, styleNode, depth + 1, style, L);

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

void simultaneous_traversal(cairo_t *cr, cJSON *content, cJSON *stylesheet, lua_State *L) {
  /*
   * Apply default styling rules.
   */
  struct style style = {0};
  style.size = 12;
  style.a = 1;
  style.lineHeight = 1.5;
  strcpy(style.face, "Sans");
  _simultaneous_traversal(cr, content, stylesheet, 0, style, L);
}
