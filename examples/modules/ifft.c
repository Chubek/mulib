#define MULIB_IMPL_FFT
#define MULIB_IMPL_IFFT
#include "mulib-ifft.h"
#include <assert.h>
int main(void) {
  mulib_complex_t a[] = {{1, 0}, {1, 0}, {1, 0}, {1, 0}};
  assert(mulib_ifft(a, 4) && a[0].real == 1);
  return 0;
}
