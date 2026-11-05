#define MULIB_IMPL_WEBIDL
#define MULIB_IMPL_DBSCHEMA
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-webidl.h"
int
main (int argc, char **argv)
{
  const char sample[] = "dictionary Person { required DOMString name; "
                        "unsigned long age = 0; };\n";
  const char *source = sample;
  size_t length = sizeof sample - 1;
  mulib_webidl_options_t options = { MULIB_WEBIDL_SEXPR, MULIB_WEBIDL_AST };
  mulib_webidl_error_t error;
  mulib_buffer_t input = { 0 }, output = { 0 };
  int result = 1;
  if (argc > 3)
    {
      fprintf (stderr, "usage: %s [--ast|--cst|--yaml|--c99] [file.idl]\n",
               argv[0]);
      return 1;
    }
  if (argc > 1)
    {
      if (!strcmp (argv[1], "--cst"))
        options.tree = MULIB_WEBIDL_CST;
      else if (!strcmp (argv[1], "--yaml"))
        options.format = MULIB_WEBIDL_CODEYAML;
      else if (!strcmp (argv[1], "--c99"))
        options.format = MULIB_WEBIDL_C99;
      else if (strcmp (argv[1], "--ast"))
        {
          fprintf (stderr, "unknown format: %s\n", argv[1]);
          return 1;
        }
    }
  if (argc == 3)
    {
      unsigned char chunk[4096];
      size_t n;
      FILE *file = fopen (argv[2], "rb");
      int read_ok = 1;
      if (!file)
        {
          perror (argv[2]);
          return 1;
        }
      while ((n = fread (chunk, 1, sizeof chunk, file)) != 0)
        if (input.length + n > 16777216
            || !mulib_buffer_append (&input, chunk, n))
          {
            read_ok = 0;
            break;
          }
      if (ferror (file))
        read_ok = 0;
      if (fclose (file))
        read_ok = 0;
      if (!read_ok)
        {
          fprintf (stderr, "cannot read WebIDL (16 MiB limit)\n");
          goto done;
        }
      source = (const char *)input.data;
      length = input.length;
    }
  if (!mulib_webidl_compile (source, length, &options, &output, &error))
    {
      fprintf (stderr, "%zu:%zu: %s\n", error.line, error.column,
               error.message);
      goto done;
    }
  result = fwrite (output.data, 1, output.length, stdout) == output.length ? 0
                                                                           : 1;
done:
  mulib_buffer_free (&input);
  mulib_buffer_free (&output);
  return result;
}
