#ifndef _I8085_STDDEF_H_
#define _I8085_STDDEF_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __WCHAR_TYPE__ wchar_t;

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _I8085_STDDEF_H_
