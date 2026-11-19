/* Stable, translation-unit independent identities for typed overlay widgets. */
#ifndef MULIB_RETGTK_KIND_H
#define MULIB_RETGTK_KIND_H
/* Zero and small legacy kind values are deliberately not type identities.
 * Custom factories may reserve their own values >= MRET_KIND_USER. */
enum {
    MRET_KIND_BASE = 0x4d520000,
    MRET_KIND_LABEL,
    MRET_KIND_BUTTON,
    MRET_KIND_TOGGLE,
    MRET_KIND_ENTRY,
    MRET_KIND_LIST,
    MRET_KIND_TABLEVIEW,
    MRET_KIND_CONSTRAINT,
    MRET_KIND_USER = 0x60000000
};
#endif
