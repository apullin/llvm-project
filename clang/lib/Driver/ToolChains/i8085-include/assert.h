#ifndef _I8085_ASSERT_H_
#define _I8085_ASSERT_H_

#ifdef __cplusplus
extern "C" {
#endif

void __assert_fail(const char *expr, const char *file, unsigned int line,
                   const char *func) __attribute__((noreturn));

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr)                                                      \
  ((expr) ? (void)0                                                       \
          : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

#endif // _I8085_ASSERT_H_
