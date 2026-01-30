#ifndef _I8085_STRING_H_
#define _I8085_STRING_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _I8085_STRING_H_
