#include <assert.h>
#include <stdint.h>
extern uint64_t backend_check (uint64_t, uint64_t);
extern uint64_t backend_macro (uint64_t, uint64_t);
extern uint64_t backend_memory (uint64_t *, uint64_t);
uint64_t backend_host (uint64_t *p, uint64_t x)
{ *p += x; return *p + 5; }
int main (void)
{
  uint64_t seed = UINT64_C(0x1234567abcdef);
  for (unsigned k = 0; k < 20000; ++k) {
    uint64_t x, y, expected;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; x = seed;
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; y = seed;
    expected = ((x * 2) ^ (x + y)) << (y & 63);
    assert (backend_check (x,y) == expected);
    assert (backend_macro (x,y) == x + 24 * y);
    {
      uint64_t cell = 999;
      assert (backend_memory (&cell,x) == 4 * x + 5);
      assert (cell == 2 * x);
    }
  }
  return 0;
}
