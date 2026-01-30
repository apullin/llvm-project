// Minimal freestanding libc helpers for i8085.
// No headers: keep this self-contained and avoid host includes.

typedef unsigned int size_t;

typedef unsigned char u8;

typedef void (*atexit_fn)(void *);

void *memcpy(void *dst, const void *src, size_t n) {
  u8 *d = (u8 *)dst;
  const u8 *s = (const u8 *)src;
  for (size_t i = 0; i < n; ++i)
    d[i] = s[i];
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  u8 *d = (u8 *)dst;
  const u8 *s = (const u8 *)src;
  if (d == s || n == 0)
    return dst;
  if (d < s) {
    for (size_t i = 0; i < n; ++i)
      d[i] = s[i];
  } else {
    for (size_t i = n; i != 0; --i)
      d[i - 1] = s[i - 1];
  }
  return dst;
}

void *memset(void *dst, int c, size_t n) {
  u8 *d = (u8 *)dst;
  u8 v = (u8)c;
  for (size_t i = 0; i < n; ++i)
    d[i] = v;
  return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
  const u8 *x = (const u8 *)a;
  const u8 *y = (const u8 *)b;
  for (size_t i = 0; i < n; ++i) {
    if (x[i] != y[i])
      return (int)x[i] - (int)y[i];
  }
  return 0;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n] != 0)
    ++n;
  return n;
}

__attribute__((weak)) void abort(void) {
  for (;;)
    ;
}

__attribute__((weak)) void __stack_chk_fail(void) {
  abort();
}

__attribute__((weak)) int __cxa_atexit(atexit_fn func, void *arg, void *dso) {
  (void)func;
  (void)arg;
  (void)dso;
  return 0;
}

__attribute__((weak)) void __cxa_finalize(void *f) {
  (void)f;
}

__attribute__((weak)) void __assert_fail(const char *expr, const char *file,
                                         unsigned int line, const char *func) {
  (void)expr;
  (void)file;
  (void)line;
  (void)func;
  abort();
}

__attribute__((weak)) void *__dso_handle = 0;
