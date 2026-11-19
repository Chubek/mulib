#include "mulib-nb-core.h"

uint64_t notebook_desktop_seed(mulib_nb *doc)
{
    return mulib_nb_insert(doc, 0, MULIB_NB_MARKDOWN, "Example", "", 32.0);
}
