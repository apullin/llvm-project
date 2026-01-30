// REQUIRES: i8085-registered-target
// RUN: %clang --target=i8085-unknown-elf -ffreestanding -fsyntax-only %s

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int va_sum(int count, ...) {
  va_list ap;
  int sum = 0;
  va_start(ap, count);
  for (int i = 0; i < count; ++i)
    sum += va_arg(ap, int);
  va_end(ap);
  return sum;
}

int sysroot_headers_smoke(uint32_t x, const char *p) {
  alignas(2) uint8_t buf[8];
  size_t n = sizeof(buf);
  memset(buf, 0, n);
  if (p)
    memcpy(buf, p, 4);

  if (x > UINT32_MAX)
    return -1;

  float f = FLT_MIN;
  (void)f;
  assert(n != 0);
  return (int)buf[0] + va_sum(1, (int)x);
}
