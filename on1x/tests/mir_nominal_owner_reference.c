/* Independent C11 model for the nominal-owner MIR qualification fixture. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct local {
    uint64_t nominal, place;
    int64_t value;
};

static void record(const char *op, uint64_t nominal, uint64_t from, uint64_t to, int64_t value)
{
    fprintf(stderr, "owner.%s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRId64 "\n",
            op, nominal, from, to, value);
}

static void create(struct local *cell, uint64_t nominal, uint64_t place, int64_t value)
{
    if (cell->place != 0 || place == 0)
        abort();
    *cell = (struct local){nominal, place, value};
    record("create", nominal, 0, place, value);
}

static void move(struct local *cell, uint64_t from, uint64_t to)
{
    if (cell->place != from || from == 0 || to == 0 || from == to)
        abort();
    cell->place = to;
    record("move", cell->nominal, from, to, cell->value);
}

static void drop(struct local *cell, uint64_t from)
{
    if (cell->place != from || from == 0)
        abort();
    record("drop", cell->nominal, from, 0, cell->value);
    *cell = (struct local){0};
}

int main(int argc, char **argv)
{
    const bool trap = argc == 2 && strcmp(argv[1], "trap") == 0;
    const int64_t subtotal = INT64_C(6) * 7;
    printf("%" PRId64 "\n", subtotal);
    struct local cell = {0};
    create(&cell, 30, 11, subtotal);
    move(&cell, 11, 13);
    move(&cell, 13, 15);
    drop(&cell, 15);
    create(&cell, 31, 20, -9);
    drop(&cell, 20);
    const int64_t total = subtotal - 8;
    printf("%" PRId64 "\n", total);
    if (trap) {
        fputs("On1x assertion failed\n", stderr);
        return 70;
    }
    return (int)total;
}
