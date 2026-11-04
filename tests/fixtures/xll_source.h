static const char xll_source[] =
    "xll 1 {\n"
    "/* properties can appear in any order */\n"
    "layout greeting { text \"Hello سلام world\"; font \"DejaVu Sans\" 18; language \"ar\"; "
    "width 93.25; spacing 1.125; origin -20.5 4e1; wrap word_grapheme; align end; direction auto_rtl; }\n"
    "layout bytes {text \"a\\u0000b\\n\\\"\\\\?" "?/\\uD83D\\uDE00\"; font \"sans\" 18; wrap none;}\n"
    "layout empty { font \"sans\" 18; direction rtl; origin -0 -0; }\n"
    "// last comment\n}\n";
