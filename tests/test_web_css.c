#include "fixtures/web_primitives_impl.h"
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#include "mulib-css.h"
#include <assert.h>
#include <float.h>

static int
token (void *p, const mulib_css_token_t *t)
{
  assert (t->length);
  (*(size_t *)p)++;
  return 1;
}
static int
layout (void *p, size_t index, const mulib_css_box_t *box)
{
  assert (index == *(size_t *)p && mulib_css_box_valid (*box));
  (*(size_t *)p)++;
  return 1;
}
int
main (void)
{
  mulib_xml_error_t native_error;
  mulib_css_sheet_t *native_sheet = mulib_css_parse ("p{color:red}",12,&native_error);
  assert (native_sheet && mulib_css_rule_count(native_sheet)==1);
  mulib_css_destroy(native_sheet);
  size_t count = 0;
  const char *source = "/* comment */ .x { width: -1.5e2px; content: '}'; }";
  assert (
      mulib_css_tokenizer_scan (source, strlen (source), token, &count, 100));
  assert (!mulib_css_tokenizer_scan ("/*", 2, token, &count, 100));
  assert (!mulib_css_tokenizer_scan ("'bad", 4, token, &count, 100));
  assert (
      !mulib_css_tokenizer_scan (source, strlen (source), token, &count, 1));
  mulib_css_parsed_sheet_t sheet = { 0 };
  source = "/*lead*/ * { color: black; } .featured { width : 12px; color: "
           "blue; } #price { color: red; } .featured { color: green; }";
  assert (mulib_css_parse_sheet (source, strlen (source), 4, &sheet)
          && sheet.len == 4);
  mulib_html_tree_node_t *root = NULL;
  const char *html = "<p class='a featured b' id='price'>hi</p>";
  assert (mulib_html_tree_parse (html, strlen (html), 10, 5, &root));
  mulib_html_tree_node_t *p = root->first_child;
  assert (mulib_css_selector_matches (p, ".featured")
          && !mulib_css_selector_matches (p, ".feature"));
  assert (mulib_css_selector_specificity ("#price") == 100
          && mulib_css_selector_specificity ("p > a") == -1);
  mulib_css_computed_t computed;
  assert (mulib_css_cascade_compute (&sheet, p, NULL, &computed));
  assert (!strcmp (computed.color, "red") && computed.width_px == 12);
  const char *declarations = "width : 20px; height: auto; font-size: 18px";
  assert (
      mulib_css_computed_apply (&computed, declarations, strlen (declarations))
      && computed.width_px == 20 && computed.height_px == -1
      && computed.font_size_px == 18);
  mulib_css_computed_t saved = computed;
  declarations = "color: blue; width: bananas";
  assert (!mulib_css_computed_apply (&computed, declarations,
                                     strlen (declarations))
          && !memcmp (&saved, &computed, sizeof saved));
  assert (!mulib_css_computed_apply (&computed, "width: px", 9));
  mulib_css_sheet_free (&sheet);
  mulib_html_tree_free (root);
  const char *bad[]
      = { "p { color:red",        "p{a:{b}}", "@media x {p{a:b}}",
          "p{color:red} garbage", "/*bad",    "p{color:'bad}" };
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
    assert (!mulib_css_parse_sheet (bad[i], strlen (bad[i]), 10, &sheet));
  source = "p { content: 'escaped \\' brace }'; color: red; }";
  assert (mulib_css_parse_sheet (source, strlen (source), 1, &sheet));
  mulib_css_sheet_free (&sheet);
  mulib_css_box_t parent = { 10, 20, 100, 100 }, boxes[4];
  double heights[] = { 10, 20 }, weights[] = { 1, 3 };
  assert (mulib_css_box_vertical (parent, heights, 2, 5, boxes)
          && boxes[1].y == 35);
  assert (mulib_css_box_contains (parent, 10, 20)
          && !mulib_css_box_contains (parent, 110, 20));
  boxes[0].x = 99;
  heights[1] = 100;
  assert (!mulib_css_box_vertical (parent, heights, 2, 5, boxes)
          && boxes[0].x == 99);
  assert (mulib_css_flex_distribute (parent, weights, 2, 0, 1, boxes)
          && boxes[0].width == 25 && boxes[1].width == 75);
  assert (!mulib_css_flex_distribute (parent, weights, 2, 101, 1, boxes));
  assert (mulib_css_grid_cells (parent, 2, 2, 4, 4, boxes, 4)
          && boxes[3].x == 62 && boxes[3].y == 72);
  assert (!mulib_css_grid_cells (parent, SIZE_MAX, 2, 0, 0, boxes, 4));
  count = 0;
  assert (mulib_css_layoutstream_emit (boxes, 4, layout, &count)
          && count == 4);
  boxes[3].x = NAN;
  count = 0;
  assert (!mulib_css_layoutstream_emit (boxes, 4, layout, &count)
          && count == 0);
  parent.x = NAN;
  assert (!mulib_css_grid_cells (parent, 1, 1, 0, 0, boxes, 4));
  parent = (mulib_css_box_t){ DBL_MAX, 0, DBL_MAX, 1 };
  assert (!mulib_css_box_valid (parent));
  return 0;
}
