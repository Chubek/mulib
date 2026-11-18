/* Inject only adapter snapshot allocation failures. The separately compiled
 * MuLIB implementation retains its allocator. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int remaining = -1;
static size_t live;

static void *
checked_malloc(size_t size)
{
  void *value;
  if (!remaining)
    return NULL;
  if (remaining > 0)
    --remaining;
  value = malloc(size);
  if (value)
    ++live;
  return value;
}

static void
checked_free(void *value)
{
  if (value)
    {
      assert(live);
      --live;
      free(value);
    }
}

#define ON1X_TTY_MALLOC checked_malloc
#define ON1X_TTY_FREE checked_free
#define ON1X_TTY_POOL_BYTES 128u
#include "../stdlib/tty/runtime.c"

static void
check_cleanup(void)
{
  assert(live == 0);
}

int
main(void)
{
  const char *value;
  size_t retained;
  assert(!atexit(check_cleanup));
  remaining = 0;
  value = on1x_tty_render(1, "allocation", 0, -1, -1, false);
  assert(!strcmp(value, ""));
  assert(on1x_tty_status() == ON1X_TTY_MEMORY);
  assert(live == 0);

  remaining = -1;
  value = on1x_tty_render(1, "snapshot", 0, -1, -1, false);
  assert(!strcmp(value, "snapshot"));
  assert(on1x_tty_status() == ON1X_TTY_PLAIN);
  assert(live == 1);
  while (on1x_tty_status() != ON1X_TTY_LIMIT)
    (void)on1x_tty_render(1, "0123456789abcdef", 0, -1, -1, false);
  retained = live;
  assert(!strcmp(on1x_tty_render(1, "more", 0, -1, -1, false), ""));
  assert(on1x_tty_status() == ON1X_TTY_LIMIT);
  assert(live == retained);
  return 0;
}
