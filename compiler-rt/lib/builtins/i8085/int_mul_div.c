// Minimal i8085 integer libcalls for mul/div/rem.
// These avoid using native *, /, % to prevent recursive libcalls.

#include <stdint.h>

static uint32_t i8085_udivmod32(uint32_t num, uint32_t den, uint32_t *rem) {
  if (den == 0) {
    if (rem)
      *rem = 0;
    return 0;
  }

  uint32_t q = 0;
  uint32_t r = 0;
  for (int i = 31; i >= 0; --i) {
    r = (r << 1) | ((num >> i) & 1u);
    if (r >= den) {
      r -= den;
      q |= (1u << i);
    }
  }

  if (rem)
    *rem = r;
  return q;
}

static uint32_t i8085_abs_u32(int32_t v) {
  return v < 0 ? (uint32_t)(0u - (uint32_t)v) : (uint32_t)v;
}

uint8_t __mul8(uint8_t a, uint8_t b) {
  uint16_t res = 0;
  uint16_t aa = a;
  uint16_t bb = b;
  while (bb) {
    if (bb & 1u)
      res += aa;
    aa <<= 1;
    bb >>= 1;
  }
  return (uint8_t)res;
}

uint16_t __mul16(uint16_t a, uint16_t b) {
  uint32_t res = 0;
  uint32_t aa = a;
  uint32_t bb = b;
  while (bb) {
    if (bb & 1u)
      res += aa;
    aa <<= 1;
    bb >>= 1;
  }
  return (uint16_t)res;
}

uint32_t __mul32(uint32_t a, uint32_t b) {
  uint32_t res = 0;
  uint32_t aa = a;
  uint32_t bb = b;
  while (bb) {
    if (bb & 1u)
      res += aa;
    aa <<= 1;
    bb >>= 1;
  }
  return res;
}

uint8_t __udiv8(uint8_t a, uint8_t b) {
  if (b == 0)
    return 0;
  return (uint8_t)i8085_udivmod32(a, b, 0);
}

uint16_t __udiv16(uint16_t a, uint16_t b) {
  if (b == 0)
    return 0;
  return (uint16_t)i8085_udivmod32(a, b, 0);
}

uint32_t __udiv32(uint32_t a, uint32_t b) {
  if (b == 0)
    return 0;
  return i8085_udivmod32(a, b, 0);
}

uint8_t __urem8(uint8_t a, uint8_t b) {
  if (b == 0)
    return 0;
  uint32_t r = 0;
  (void)i8085_udivmod32(a, b, &r);
  return (uint8_t)r;
}

uint16_t __urem16(uint16_t a, uint16_t b) {
  if (b == 0)
    return 0;
  uint32_t r = 0;
  (void)i8085_udivmod32(a, b, &r);
  return (uint16_t)r;
}

uint32_t __urem32(uint32_t a, uint32_t b) {
  if (b == 0)
    return 0;
  uint32_t r = 0;
  (void)i8085_udivmod32(a, b, &r);
  return r;
}

int8_t __sdiv8(int8_t a, int8_t b) {
  int32_t qa = (int32_t)a;
  int32_t qb = (int32_t)b;
  if (qb == 0)
    return 0;
  int neg = (qa < 0) ^ (qb < 0);
  uint32_t ua = i8085_abs_u32(qa);
  uint32_t ub = i8085_abs_u32(qb);
  uint32_t q = i8085_udivmod32(ua, ub, 0);
  int32_t res = (int32_t)q;
  return (int8_t)(neg ? -res : res);
}

int16_t __sdiv16(int16_t a, int16_t b) {
  int32_t qa = (int32_t)a;
  int32_t qb = (int32_t)b;
  if (qb == 0)
    return 0;
  int neg = (qa < 0) ^ (qb < 0);
  uint32_t ua = i8085_abs_u32(qa);
  uint32_t ub = i8085_abs_u32(qb);
  uint32_t q = i8085_udivmod32(ua, ub, 0);
  int32_t res = (int32_t)q;
  return (int16_t)(neg ? -res : res);
}

int32_t __sdiv32(int32_t a, int32_t b) {
  if (b == 0)
    return 0;
  int neg = (a < 0) ^ (b < 0);
  uint32_t ua = i8085_abs_u32(a);
  uint32_t ub = i8085_abs_u32(b);
  uint32_t q = i8085_udivmod32(ua, ub, 0);
  int32_t res = (int32_t)q;
  return neg ? -res : res;
}

int8_t __srem8(int8_t a, int8_t b) {
  int32_t qa = (int32_t)a;
  int32_t qb = (int32_t)b;
  if (qb == 0)
    return 0;
  uint32_t ua = i8085_abs_u32(qa);
  uint32_t ub = i8085_abs_u32(qb);
  uint32_t r = 0;
  (void)i8085_udivmod32(ua, ub, &r);
  int32_t res = (int32_t)r;
  return (int8_t)((qa < 0) ? -res : res);
}

int16_t __srem16(int16_t a, int16_t b) {
  int32_t qa = (int32_t)a;
  int32_t qb = (int32_t)b;
  if (qb == 0)
    return 0;
  uint32_t ua = i8085_abs_u32(qa);
  uint32_t ub = i8085_abs_u32(qb);
  uint32_t r = 0;
  (void)i8085_udivmod32(ua, ub, &r);
  int32_t res = (int32_t)r;
  return (int16_t)((qa < 0) ? -res : res);
}

int32_t __srem32(int32_t a, int32_t b) {
  if (b == 0)
    return 0;
  uint32_t ua = i8085_abs_u32(a);
  uint32_t ub = i8085_abs_u32(b);
  uint32_t r = 0;
  (void)i8085_udivmod32(ua, ub, &r);
  int32_t res = (int32_t)r;
  return (a < 0) ? -res : res;
}
