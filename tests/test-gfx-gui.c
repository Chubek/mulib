/* Local GUI helpers: no desktop server, device, D-Bus or subprocess required. */
#define MULIB_GUI_RESOURCE_LIMIT 8u
#define MULIB_GUI_CLIPBOARD_LIMIT 64u
#define MULIB_GUI_SETTINGS_LIMIT 16u
#define MULIB_GUI_SETTINGS_INPUT_LIMIT 1024u
#define MULIB_GUI_DESKTOP_INPUT_LIMIT 2048u
#include <stdlib.h>
#include <assert.h>
static int fail_allocation;
static void *test_malloc(size_t n) { return fail_allocation ? NULL : malloc(n); }
static void *test_realloc(void *p, size_t n) { return fail_allocation ? NULL : realloc(p, n); }
#define malloc test_malloc
#define realloc test_realloc
#include "mulib-gui-resource.h"
#include "mulib-gui-dragdrop.h"
#include "mulib-gui-settings.h"
#include "mulib-gui-desktop.h"
#undef malloc
#undef realloc

typedef struct {
    mulib_gui_resource_t *registry;
    int released, reenter, blocked;
} resource_fixture;

static void release_fixture(void *data)
{
    resource_fixture *f = data;
    f->released++;
    if (f->reenter) {
        assert(f->registry->closing);
        assert(!mulib_gui_resource_acquire(f->registry, 1));
        assert(!mulib_gui_resource_release(f->registry, 1));
        f->blocked = !mulib_gui_resource_add(f->registry, 99, f, NULL);
        mulib_gui_resource_free(f->registry);
    }
}

static void test_resources(void)
{
    mulib_gui_resource_t r = {0};
    resource_fixture a = {&r, 0, 0, 0}, b = {&r, 0, 1, 0};
    assert(!mulib_gui_resource_add(NULL, 1, &a, release_fixture));
    assert(!mulib_gui_resource_add(&r, 0, &a, release_fixture));
    fail_allocation = 1;
    assert(!mulib_gui_resource_add(&r, 1, &a, release_fixture));
    fail_allocation = 0;
    assert(!r.v && !r.n && !a.released);
    assert(mulib_gui_resource_add(&r, 1, &a, release_fixture));
    assert(!mulib_gui_resource_add(&r, 1, &b, release_fixture));
    assert(mulib_gui_resource_acquire(&r, 1) == &a);
    assert(mulib_gui_resource_release(&r, 1) && !a.released);
    assert(mulib_gui_resource_release(&r, 1) && a.released == 1);
    assert(!mulib_gui_resource_release(&r, 1));
    assert(mulib_gui_resource_add(&r, 1, &a, release_fixture));
    r.v[0].refs = SIZE_MAX;
    assert(!mulib_gui_resource_acquire(&r, 1));
    assert(r.v[0].refs == SIZE_MAX);
    assert(mulib_gui_resource_add(&r, 2, &b, release_fixture));
    mulib_gui_resource_free(&r);
    assert(a.released == 2 && b.released == 1 && b.blocked);
    assert(!r.v && !r.n && !r.cap && !r.closing);
    mulib_gui_resource_free(&r);
    assert(b.released == 1);
    for (uint64_t i = 1; i <= MULIB_GUI_RESOURCE_LIMIT; i++)
        assert(mulib_gui_resource_add(&r, i, &a, NULL));
    assert(!mulib_gui_resource_add(&r, 50, &a, NULL));
    mulib_gui_resource_free(&r);
    mulib_gui_resource_free(NULL);
}

static void test_clipboard(void)
{
    mulib_gui_clipboard_t c = {0};
    size_t n = 100;
    const char payload[] = "text/plain\0payload";
    assert(mulib_gui_clipboard_set(&c, "text/plain", payload, sizeof payload));
    assert(c.serial == 1);
    unsigned char *old_data = c.data;
    fail_allocation = 1;
    assert(!mulib_gui_clipboard_set(&c, "text/plain", "new", 3));
    fail_allocation = 0;
    assert(c.data == old_data && c.serial == 1 && c.n == sizeof payload);
    assert(mulib_gui_clipboard_get(&c, "text/plain", &n) == c.data && n == sizeof payload);
    assert(!mulib_gui_clipboard_get(&c, "text/html", &n) && n == 0);
    assert(!mulib_gui_clipboard_set(&c, "bad\nmime", "x", 1));
    assert(!mulib_gui_clipboard_set(&c, "text/plain", NULL, 1));
    assert(!mulib_gui_clipboard_set(&c, "text/plain", "x", MULIB_GUI_CLIPBOARD_LIMIT+1));
    assert(c.serial == 1 && c.n == sizeof payload);
    /* Both inputs borrow the very buffer which set will replace. */
    assert(mulib_gui_clipboard_set(&c, (const char *)c.data, c.data, c.n));
    assert(!memcmp(c.data, payload, sizeof payload) && c.serial == 2);
    assert(mulib_gui_clipboard_set(&c, c.mime, c.data+11, 7));
    assert(!memcmp(c.data, "payload", 7));
    assert(mulib_gui_clipboard_set(&c, "text/plain; charset=utf-8", NULL, 0));
    assert(mulib_gui_clipboard_get(&c, c.mime, &n) && n == 0);
    c.serial = UINT64_MAX;
    assert(!mulib_gui_clipboard_set(&c, "text/plain", NULL, 0));
    assert(c.serial == UINT64_MAX);
    mulib_gui_clipboard_free(&c);
    mulib_gui_clipboard_free(&c);
    mulib_gui_clipboard_free(NULL);
    assert(!mulib_gui_clipboard_get(NULL, "text/plain", &n) && !n);

    mulib_gui_dragdrop_t d = {0};
    uint64_t target = 999;
    assert(!mulib_gui_dragdrop_end(&d, &target) && !target);
    assert(mulib_gui_dragdrop_begin(&d, 1, "text/plain", "abc", 3));
    assert(!mulib_gui_dragdrop_begin(&d, 2, "text/plain", "abc", 3));
    assert(mulib_gui_dragdrop_hover(&d, 2, "text/plain"));
    assert(!mulib_gui_dragdrop_hover(&d, 3, "image/png"));
    assert(!mulib_gui_dragdrop_end(&d, &target) && !target);
    assert(mulib_gui_dragdrop_begin(&d, 1, "text/plain", "xyz", 3));
    assert(mulib_gui_dragdrop_hover(&d, 7, "text/plain"));
    assert(mulib_gui_dragdrop_end(&d, &target) && target == 7);
    assert(!mulib_gui_dragdrop_end(&d, &target) && !target);
    mulib_gui_dragdrop_free(&d);
}

static FILE *stream_bytes(const void *bytes, size_t n)
{
    FILE *f = tmpfile();
    assert(f);
    assert(fwrite(bytes, 1, n, f) == n);
    rewind(f);
    return f;
}

static FILE *stream_text(const char *s)
{
    return stream_bytes(s, strlen(s));
}

static void test_settings(void)
{
    mulib_gui_settings_t s = {0};
    assert(!mulib_gui_settings_set(NULL, "a", "b"));
    assert(mulib_gui_settings_set(&s, "a", "old"));
    FILE *allocation_stream = stream_text("a=allocation failure\n");
    fail_allocation = 1;
    assert(!mulib_gui_settings_read(&s, allocation_stream));
    fail_allocation = 0; fclose(allocation_stream);
    assert(!strcmp(mulib_gui_settings_get(&s, "a"), "old"));
    FILE *f = stream_text("a=new\r\nb=two\nb=last");
    assert(mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, "a"), "new"));
    assert(!strcmp(mulib_gui_settings_get(&s, "b"), "last") && s.n == 2);
    f = stream_text("a=bad\nmissing separator\n");
    assert(!mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, "a"), "new") && s.n == 2);
    const char nul[] = "a=bad\nb=ok\0hidden=value\n";
    f = stream_bytes(nul, sizeof nul-1);
    assert(!mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, "b"), "last"));

    char key[64], value[256], line[400];
    memset(key, 'k', sizeof key-1); key[sizeof key-1] = 0;
    memset(value, 'v', sizeof value-1); value[sizeof value-1] = 0;
    assert(mulib_gui_settings_set(&s, key, value));
    snprintf(line, sizeof line, "%s=%s\r\n", key, value);
    f = stream_text(line);
    assert(mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, key), value));
    memset(line, 'x', sizeof line); line[0]='a'; line[1]='=';
    f = stream_bytes(line, sizeof line);
    assert(!mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, "a"), "new"));
    f = tmpfile(); assert(f);
    for (size_t i = 0; i <= MULIB_GUI_SETTINGS_INPUT_LIMIT/4; i++)
        assert(fputs("a=x\n", f) >= 0);
    rewind(f); assert(!mulib_gui_settings_read(&s, f)); fclose(f);
    assert(!strcmp(mulib_gui_settings_get(&s, "a"), "new"));

    /* Force growth while the value argument borrows the old vector. */
    while (s.n < s.cap && s.n < MULIB_GUI_SETTINGS_LIMIT) {
        char k[32]; snprintf(k, sizeof k, "key%zu", s.n);
        assert(mulib_gui_settings_set(&s, k, "value"));
    }
    const char *borrowed = mulib_gui_settings_get(&s, key);
    assert(mulib_gui_settings_set(&s, "alias", borrowed));
    assert(!strcmp(mulib_gui_settings_get(&s, "alias"), value));
    assert(mulib_gui_settings_set(&s, s.v[0].key, s.v[0].value));
    f = tmpfile(); assert(f);
    assert(mulib_gui_settings_write(&s, f)); rewind(f);
    mulib_gui_settings_t copy = {0};
    assert(mulib_gui_settings_read(&copy, f) && copy.n == s.n);
    fclose(f);
    mulib_gui_settings_free(&copy);
    while (s.n < MULIB_GUI_SETTINGS_LIMIT) {
        char k[32]; snprintf(k, sizeof k, "fill%zu", s.n);
        assert(mulib_gui_settings_set(&s, k, "v"));
    }
    assert(!mulib_gui_settings_set(&s, "over-limit", "v"));
    assert(mulib_gui_settings_set(&s, "a", "allowed replacement"));
    assert(!mulib_gui_settings_get(NULL, "a"));
    assert(!mulib_gui_settings_get(&s, NULL));
    mulib_gui_settings_free(&s); mulib_gui_settings_free(&s);
}

static void desktop_reject(mulib_gui_desktop_entry_t *d, const void *p, size_t n)
{
    mulib_gui_desktop_entry_t old = *d;
    FILE *f = stream_bytes(p, n);
    assert(!mulib_gui_desktop_parse(d, f)); fclose(f);
    assert(!memcmp(&old, d, sizeof old));
}

static void test_desktop(void)
{
    mulib_gui_desktop_entry_t d = {0};
    FILE *f = stream_text("# comment\r\n[Desktop Entry]\r\nName=Hello\\sWorld\n"
                          "Name[fr]=Bonjour\nType=Application\nIcon=a\\\\b\n"
                          "Exec=tool \"literal;$(not-run)\" %F\nHidden=false\nTerminal=true\n"
                          "[Desktop Action Test]\nName=Other\n");
    assert(mulib_gui_desktop_parse(&d, f)); fclose(f);
    assert(!strcmp(d.name, "Hello World") && !strcmp(d.icon, "a\\b"));
    assert(!strcmp(d.exec, "tool \"literal;$(not-run)\" %F"));
    assert(!d.hidden && d.terminal);
    const char *bad[] = {
        "[Desktop Entry]\nName=A\nName=B\nType=Application\n",
        "[Desktop Entry]\nName=A\nType=Application\nHidden=TRUE\n",
        "[Desktop Entry]\nName=A\\q\nType=Application\n",
        "[Desktop Entry]\nName=A\\\nType=Application\n",
        "[Desktop Entry]\nName=A\nType=Application\n[Desktop Entry]\n",
        "[Desktop Entry]\nName=A\nType=Application\nbroken\n",
        "[Desktop Entry]\nName=A\n",
    };
    for (size_t i = 0; i < sizeof bad/sizeof *bad; i++)
        desktop_reject(&d, bad[i], strlen(bad[i]));
    char long_exec[700];
    const char *prefix = "[Desktop Entry]\nName=A\nType=Application\nExec=";
    size_t pn = strlen(prefix);
    memcpy(long_exec, prefix, pn); memset(long_exec+pn, 'x', sizeof long_exec-pn);
    desktop_reject(&d, long_exec, sizeof long_exec);
    const char nul[] = "[Desktop Entry]\nName=A\0B\nType=Application\n";
    desktop_reject(&d, nul, sizeof nul-1);
    f = tmpfile(); assert(f);
    assert(fputs("[Desktop Entry]\nName=A\nType=Application\n", f) >= 0);
    for (size_t i = 0; i < MULIB_GUI_DESKTOP_INPUT_LIMIT; i++) assert(fputc('\n', f) != EOF);
    rewind(f);
    mulib_gui_desktop_entry_t old = d;
    assert(!mulib_gui_desktop_parse(&d, f)); fclose(f);
    assert(!memcmp(&old, &d, sizeof d));
}

static void test_mime(void)
{
    assert(mulib_gui_mime_valid("text/plain; charset=utf-8"));
    assert(mulib_gui_mime_valid("application/x.test+json"));
    assert(!mulib_gui_mime_valid(NULL));
    assert(!mulib_gui_mime_valid("text"));
    assert(!mulib_gui_mime_valid("text/"));
    assert(!mulib_gui_mime_valid("text/*"));
    assert(!mulib_gui_mime_valid("text/plain\r\ninjected: yes"));
    assert(!mulib_gui_mime_valid("text/plain;foo="));
    assert(!strcmp(mulib_gui_mime_guess("x.JSON", NULL, 0), "application/json"));
    assert(!strcmp(mulib_gui_mime_guess("dir.png/file", NULL, 0), "application/octet-stream"));
    assert(!strcmp(mulib_gui_mime_guess("x.txt", "\x89PNG\r\n\x1a\n", 8), "image/png"));
    assert(!strcmp(mulib_gui_mime_guess(NULL, "RIFFxxxxWEBP", 12), "image/webp"));
    assert(!strcmp(mulib_gui_mime_guess(NULL, "GIF89a", 6), "image/gif"));
    assert(!strcmp(mulib_gui_mime_guess(NULL, NULL, SIZE_MAX), "application/octet-stream"));
}

int main(void)
{
    test_resources(); test_clipboard(); test_settings(); test_desktop(); test_mime();
    puts("gfx GUI local helpers: ok");
    return 0;
}
