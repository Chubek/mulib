# Lexicographic byte ordering

Define `MULIB_IMPL_LEXOGRAPHIC_SORT`.
[Example](../examples/modules/lexographic_sort.c). Filename spelling retained.

Compare NUL-terminated byte strings as unsigned bytes, optionally folding ASCII
A-Z to a-z. Comparator returns negative/zero/positive; NULL sorts before strings.
Sort rejects NULL entries (or missing nonempty array) before changing the array.
It stably rearranges caller-owned pointers, never copies or frees strings.
Empty arrays may be NULL. Insertion sort takes O(n²) comparisons and constant
storage. This is locale-independent byte ordering, not Unicode collation or
natural numeric sorting; UTF-8 bytes are not decoded.
