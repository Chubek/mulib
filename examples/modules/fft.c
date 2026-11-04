#define MULIB_IMPL_FFT
#include "mulib-fft.h"
#include <assert.h>
int main(void) {
  mulib_complex_t a[] = {{1, 0}, {0, 0}, {0, 0}, {0, 0}};
  assert(mulib_fft(a, 4));
  for (size_t i = 0; i < 4; ++i) assert(a[i].real == 1 && a[i].imag == 0);
  return 0;
}
