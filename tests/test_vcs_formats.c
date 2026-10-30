#define MULIB_IMPL_GIT
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VCSREPO
#define MULIB_IMPL_VCSCOMMIT
#define MULIB_IMPL_VCSCOMITT
#define MULIB_IMPL_VCSPACK
#define MULIB_IMPL_VCDIFF
#include "mulib-vcdiff.h"
#include "mulib-vcscomitt.h"
#include "mulib-vcspack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
hashes (void)
{
  static const char *const input[]
      = { "", "abc",
          "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" };
  static const char *const expected[]
      = { "da39a3ee5e6b4b0d3255bfef95601890afd80709",
          "a9993e364706816aba3e25717850c26c9cd0d89d",
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1" };
  mulib_git_oid_t id, parsed;
  char hex[41];
  for (size_t i = 0; i < 3; ++i)
    {
      assert (mulib_git_sha1 (input[i], strlen (input[i]), &id));
      assert (mulib_git_oid_format (&id, hex) && !strcmp (hex, expected[i]));
      assert (mulib_git_oid_parse (hex, 40, &parsed)
              && !memcmp (id.bytes, parsed.bytes, 20));
    }
  {
    char *million = malloc (1000000);
    assert (million);
    memset (million, 'a', 1000000);
    assert (mulib_git_sha1 (million, 1000000, &id));
    free (million);
    assert (mulib_git_oid_format (&id, hex)
            && !strcmp (hex, "34aa973cd4c4daa4f61eeb2bdbad27316534016f"));
  }
  assert (!mulib_git_sha1 (NULL, 1, &id));
  assert (!mulib_git_oid_parse ("xyz", 3, &id));
  assert (!mulib_git_object_id ("invalid", NULL, 0, &id));
  assert (mulib_git_object_id ("tree", NULL, 0, &id));
  assert (mulib_git_oid_format (&id, hex)
          && !strcmp (hex, "4b825dc642cb6eb9a060e54bf8d69288fbee4904"));
}
static void
pktlines (void)
{
  unsigned char wire[64];
  size_t n, used, length;
  const unsigned char *p;
  unsigned kind;
  for (unsigned i = 0; i < 4; ++i)
    {
      assert (mulib_git_pkt_encode (i, NULL, 0, wire, sizeof wire, &n));
      assert (mulib_git_pkt_decode (wire, n, &p, &length, &kind, &used) == 1
              && kind == i && used == n && length == 0);
    }
  assert (mulib_git_pkt_encode (0, "a\0b", 3, wire, sizeof wire, &n));
  for (size_t i = 0; i < n; ++i)
    assert (mulib_git_pkt_decode (wire, i, &p, &length, &kind, &used) == 0
            && !used);
  assert (mulib_git_pkt_decode ("0003", 4, &p, &length, &kind, &used) == -1);
  assert (mulib_git_pkt_decode ("ffff", 4, &p, &length, &kind, &used) == -1);
  assert (mulib_git_pkt_decode ("00z0", 4, &p, &length, &kind, &used) == -1);
  assert (!mulib_git_pkt_encode (1, "x", 1, wire, sizeof wire, &used));
}
static void
repository (void)
{
  mulib_vcsrepo_t r;
  mulib_git_oid_t a, b, id;
  mulib_vcscommit_t c = { 0 };
  char *bytes;
  size_t n;
  char data[] = "hello";
  assert (mulib_vcsrepo_init (&r, 8, 2, 2048));
  assert (mulib_vcsrepo_put (&r, "blob", data, 5, &a) == 1);
  data[0] = 'X';
  assert (!memcmp (mulib_vcsrepo_get (&r, &a)->data, "hello", 5));
  assert (mulib_vcsrepo_put (&r, "blob", "hello", 5, &b) == 1 && r.count == 1);
  assert (mulib_vcsrepo_put (&r, "tree", NULL, 0, &c.tree) == 1);
  c.author.name = "User";
  c.author.email = "user@example.test";
  c.author.timestamp = 1234;
  c.author.timezone_minutes = -90;
  c.committer = c.author;
  c.message = "message\n";
  c.message_length = 8;
  assert (mulib_vcscommit_encode (&c, &bytes, &n));
  assert (strstr (bytes, "author User <user@example.test> 1234 -0130\n"));
  assert (!memcmp (bytes + n - 9, "\nmessage\n", 9));
  free (bytes);
  assert (mulib_vcscomitt_store (&r, &c, &id));
  assert (mulib_vcsrepo_ref_update (&r, "refs/heads/main", NULL, &id) == 1);
  assert (mulib_vcsrepo_ref_update (&r, "refs/heads/main", NULL, &id) == -1);
  assert (mulib_vcsrepo_ref_update (&r, "refs/heads/main", &a, &id) == -1);
  assert (mulib_vcsrepo_ref_update (&r, "refs/heads/main", &id, &id) == 1);
  {
    const char *bad[]
        = { "main",    "refs/",     "refs/.hidden", "refs/x.lock", "refs/x..y",
            "refs//x", "refs/x@{y", "refs/x\ny",    "refs/x\\y" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i)
      assert (!mulib_vcsrepo_ref_update (&r, bad[i], NULL, &id));
  }
  c.parents = &a;
  c.parent_count = 1;
  assert (!mulib_vcscommit_store (&r, &c, &b));
  c.parents = &id;
  assert (mulib_vcscommit_store (&r, &c, &b));
  c.author.name = "Injected\nparent";
  assert (!mulib_vcscommit_encode (&c, &bytes, &n));
  mulib_vcsrepo_free (&r);
  mulib_vcsrepo_free (&r);
  assert (mulib_vcsrepo_init (&r, 1, 1, 1));
  assert (!mulib_vcsrepo_put (&r, "blob", "xx", 2, &a));
  assert (mulib_vcsrepo_put (&r, "blob", "x", 1, &a) == 1);
  assert (!mulib_vcsrepo_put (&r, "blob", "y", 1, &a));
  mulib_vcsrepo_free (&r);
}
typedef struct pack_check
{
  const mulib_vcspack_object_t *objects;
  size_t count;
} pack_check;
static int
check_object (void *ctx, const mulib_vcspack_object_t *o)
{
  pack_check *s = ctx;
  const mulib_vcspack_object_t *expected = s->objects + s->count++;
  assert (o->type == expected->type && o->length == expected->length);
  assert (!o->length || !memcmp (o->data, expected->data, o->length));
  return 0;
}
static int
stop_object (void *ctx, const mulib_vcspack_object_t *o)
{
  (void)ctx;
  (void)o;
  return 1;
}
static void
packs (void)
{
  unsigned char *pack, *large = malloc (70000), *scratch = malloc (70000);
  size_t n;
  mulib_vcspack_object_t objects[3]
      = { { 3, "hello\n", 6 }, { 2, NULL, 0 }, { 3, NULL, 70000 } };
  pack_check c = { objects, 0 };
  assert (large && scratch);
  for (size_t i = 0; i < 70000; ++i)
    large[i] = (unsigned char)i;
  objects[2].data = large;
  assert (mulib_vcspack_encode (objects, 3, &pack, &n));
  assert (mulib_vcspack_decode (pack, n, scratch, 70000, check_object, &c) == 1
          && c.count == 3);
  {
    FILE *f = fopen ("vcs-test.pack", "wb");
    assert (f);
    assert (fwrite (pack, 1, n, f) == n);
    assert (!fclose (f));
  }
  c.count = 0;
  assert (mulib_vcspack_decode (pack, n, scratch, 6, check_object, &c) == -2
          && c.count == 2);
  assert (mulib_vcspack_decode (pack, n, scratch, 70000, stop_object, NULL)
          == -3);
  for (size_t i = 0; i < 40; ++i)
    assert (mulib_vcspack_decode (pack, i, scratch, 70000, stop_object, NULL)
            == 0);
  pack[n - 1] ^= 1;
  assert (!mulib_vcspack_decode (pack, n, scratch, 70000, stop_object, NULL));
  pack[n - 1] ^= 1;
  /* Re-sign malformed zlib content to test the inner checksum/structure. */
  pack[16] ^= 1;
  {
    mulib_git_oid_t id;
    assert (mulib_git_sha1 (pack, n - 20, &id));
    memcpy (pack + n - 20, id.bytes, 20);
  }
  assert (mulib_vcspack_decode (pack, n, scratch, 70000, stop_object, NULL)
          == 0);
  free (pack);
  free (large);
  free (scratch);
}
static void
deltas (void)
{
  unsigned char wire[512], out[256];
  size_t n, used;
  /* RFC 3284 section 3's COPY/ADD/COPY/overlapping COPY/RUN example,
     hand-encoded with default-table SELF instructions. */
  const unsigned char rfc[]
      = { 0xd6, 0xc3, 0xc4, 0,   0,  1, 16, 0,  20, 28, 0, 5, 7, 3, 'w',
          'x',  'y',  'z',  'z', 20, 5, 20, 19, 12, 0,  4, 0, 4, 24 };
  assert (mulib_vcdiff_decode ("abcdefghijklmnop", 16, rfc, sizeof rfc, out,
                               sizeof out, &used)
          == 1);
  assert (used == 28 && !memcmp (out, "abcdwxyzefghefghefghefghzzzz", 28));
  for (size_t i = 0; i < sizeof rfc; ++i)
    {
      int rc = mulib_vcdiff_decode ("abcdefghijklmnop", 16, rfc, i, out,
                                    sizeof out, &used);
      assert (rc == 0 || (i == 5 && rc == 1 && used == 0));
    }
  assert (mulib_vcdiff_decode ("abcdefghijklmnop", 16, rfc, sizeof rfc, out,
                               27, &used)
          == -2);
  for (size_t length = 0; length < 200; ++length)
    {
      unsigned char source[200], target[200];
      for (size_t j = 0; j < length; ++j)
        {
          source[j] = (unsigned char)j;
          target[j] = j < length / 2 ? (unsigned char)j : 42;
        }
      assert (mulib_vcdiff_encode (source, length, target, length, wire,
                                   sizeof wire, &n));
      assert (
          mulib_vcdiff_decode (source, length, wire, n, out, sizeof out, &used)
              == 1
          && used == length);
      assert (!memcmp (out, target, length));
      assert (!mulib_vcdiff_encode (source, length, target, length, wire,
                                    n - 1, &used));
    }
  assert (mulib_vcdiff_decode (NULL, 0, "\xd6\xc3\xc4\0\1", 5, out, sizeof out,
                               &used)
          == -1);
  /* Source-window boundary crossing is invalid even though the target
     address space begins immediately after it. */
  {
    const unsigned char bad[]
        = { 0xd6, 0xc3, 0xc4, 0, 0, 1, 2, 0, 7, 4, 0, 0, 1, 1, 20, 0 };
    assert (
        mulib_vcdiff_decode ("ab", 2, bad, sizeof bad, out, sizeof out, &used)
        == 0);
  }
}
static void
delta_addressing (void)
{
  unsigned char source[1024], out[64], wire[64];
  size_t used;
  for (size_t i = 0; i < sizeof source; ++i)
    source[i] = (unsigned char)(i ^ (i >> 8));
  for (unsigned mode = 0; mode < 9; ++mode)
    {
      const unsigned char prefix[]
          = { 0xd6, 0xc3, 0xc4, 0, 0, 1, 0x88, 0, 0, 0, 8, 0, 0, 2, 0 };
      size_t pos = sizeof prefix;
      unsigned address = mode < 6 ? 4 : (mode - 6) * 256 + 4;
      memcpy (wire, prefix, sizeof prefix);
      wire[pos++] = 20; /* Prime the caches with a SELF COPY. */
      wire[pos++] = (unsigned char)(20 + 16 * mode);
      if (address >= 128)
        wire[pos++] = (unsigned char)(128 | (address >> 7));
      wire[pos++] = (unsigned char)(address & 127);
      if (mode == 1) /* HERE = 1024 source bytes + 4 target bytes. */
        {
          wire[pos++] = 0x88;
          wire[pos++] = 0;
        }
      else
        wire[pos++] = mode == 2 ? 0 : 4;
      wire[14] = (unsigned char)(pos - 17); /* Address section length. */
      wire[9] = (unsigned char)(pos - 10);  /* Delta section length. */
      assert (mulib_vcdiff_decode (source, sizeof source, wire, pos, out,
                                   sizeof out, &used)
                  == 1
              && used == 8);
      assert (!memcmp (out, source + address, 4));
      assert (!memcmp (out + 4, source + address, 4));
    }
  /* Paired ADD/COPY and COPY/ADD entries, including SAME address modes. */
  {
    const unsigned codes[] = { 163, 235, 247, 255 };
    for (size_t i = 0; i < sizeof codes / sizeof *codes; ++i)
      {
        unsigned char paired[]
            = { 0xd6, 0xc3, 0xc4, 0, 0, 1, 4, 0, 8, 5, 0, 1, 1, 1, 'Z', 0, 0 };
        paired[15] = (unsigned char)codes[i];
        assert (mulib_vcdiff_decode ("abcd", 4, paired, sizeof paired, out,
                                     sizeof out, &used)
                == 1);
        assert (used == 5 && !memcmp (out, i < 2 ? "Zabcd" : "abcdZ", 5));
      }
  }
  /* A second window borrows the first target window as its source. */
  {
    const unsigned char windows[]
        = { 0xd6, 0xc3, 0xc4, 0, 0, 0, 10, 4, 0, 4, 1, 0, 'a', 'b',
            'c',  'd',  5,    2, 4, 0, 7,  4, 0, 0, 1, 1, 20,  0 };
    assert (mulib_vcdiff_decode (NULL, 0, windows, sizeof windows, out,
                                 sizeof out, &used)
            == 1);
    assert (used == 8 && !memcmp (out, "abcdabcd", 8));
  }
}
int
main (void)
{
  hashes ();
  pktlines ();
  repository ();
  packs ();
  deltas ();
  delta_addressing ();
  return 0;
}
