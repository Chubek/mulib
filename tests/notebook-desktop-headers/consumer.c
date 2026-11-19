/* Also a standalone example: only the header gate defines TEST_PEER. */
#ifndef MULIB_NOTEBOOK_DESKTOP_TEST_PEER
#define MULIB_IMPL_UTF8
#endif
#include "mulib-nb-core.h"

#ifdef MULIB_NOTEBOOK_DESKTOP_TEST_PEER
uint64_t notebook_desktop_seed(mulib_nb *doc);
#else
static uint64_t notebook_desktop_seed(mulib_nb *doc)
{
    return mulib_nb_insert(doc, 0, MULIB_NB_MARKDOWN, "Example", "", 32.0);
}
#endif

int main(void)
{
    mulib_nb doc = {0};
    if (!mulib_nb_init(&doc, 1, "Notebook example"))
        return 1;

    uint64_t id = notebook_desktop_seed(&doc);
    int ok = id && doc.n == 1 && mulib_nb_edit(&doc, id, "Edited", "");
    mulib_nb_cell *cell = mulib_nb_find(&doc, id);
    ok = ok && cell && cell->kind == MULIB_NB_MARKDOWN && cell->height == 32.0
         && strcmp(cell->source, "Edited") == 0;
    mulib_nb_free(&doc);
    return !ok;
}
