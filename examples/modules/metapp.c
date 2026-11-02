#define MULIB_IMPL_METAPP
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_ELF
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_DSU
#include "mulib-metapp.h"
#include <stdio.h>
#include <string.h>
int main (int argc, char **argv) {
  mulib_metapp_options_t options = {0};
  mulib_metapp_t *pp;
  mulib_metapp_error_t error = {0};
  mulib_strbuf_t output = {0};
  const char *file = NULL, *dependency_root = NULL;
  int i, rc = 0;
  for (i = 1; i < argc; ++i) if (!strcmp (argv[i], "--cpp")) options.cplusplus = 1;
  pp = mulib_metapp_create (&options);
  if (!pp) return 1;
  for (i = 1; i < argc && !rc; ++i) {
    if (!strcmp (argv[i], "--cpp")) continue;
    if (!strncmp (argv[i], "-I", 2)) rc = mulib_metapp_include_path (pp, argv[i]+2);
    else if (!strncmp (argv[i], "--deps=", 7)) dependency_root = argv[i]+7;
    else if (!strncmp (argv[i], "--plugin=", 9)) rc = mulib_metapp_plugin_file (pp, argv[i]+9, &error);
    else if (!strncmp (argv[i], "-D", 2)) {
      mulib_strbuf_t definition = {0};
      char *equal;
      if (!mulib_strbuf_append (&definition, argv[i]+2)) rc = MULIB_METAPP_MEMORY;
      else {
        equal = strchr (definition.data, '=');
        if (equal) *equal = ' ';
        else if (!mulib_strbuf_append (&definition, " 1")) rc = MULIB_METAPP_MEMORY;
        if (!rc) rc = mulib_metapp_define (pp, definition.data, &error);
      }
      mulib_strbuf_free (&definition);
    } else if (argv[i][0] == '-' || file) rc = MULIB_METAPP_INVALID;
    else file = argv[i];
  }
  if (!rc && dependency_root) {
    mulib_metapp_graph_t *index = NULL, *component = NULL;
    if (!file) { fprintf (stderr, "usage: metapp --deps=PROJECT_DIRECTORY FILE\n"); rc = MULIB_METAPP_INVALID; }
    if (!rc) rc = mulib_metapp_dependencies_tree (pp, dependency_root, NULL, &index, &error);
    if (!rc) rc = mulib_metapp_graph_component (index, file, &component);
    if (rc == MULIB_METAPP_NOTFOUND) fprintf (stderr, "file is absent from the dependency index: %s\n", file);
    if (!rc) rc = mulib_metapp_graph_dot (component, &output);
    mulib_metapp_graph_destroy (component); mulib_metapp_graph_destroy (index);
  } else if (!rc && file) rc = mulib_metapp_file (pp, file, &output, &error);
  else if (!rc) {
    const char source[] = "#define CAT(a,b) a ## b\n#define VALUE 42\n"
      "#if defined(VALUE) && VALUE > 0\nint CAT(answer, _value) = VALUE;\n#endif\n";
    rc = mulib_metapp_process (pp, "example.c", source, sizeof (source)-1, &output, &error);
  }
  if (rc) fprintf (stderr, "%s:%zu:%zu: %s (status %d)\n", error.file, error.line, error.column, error.message, rc);
  else if (output.length && fwrite (output.data, 1, output.length, stdout) != output.length) rc = MULIB_METAPP_IO;
  mulib_strbuf_free (&output); mulib_metapp_destroy (pp);
  return rc ? 1 : 0;
}
