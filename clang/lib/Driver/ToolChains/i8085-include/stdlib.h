#ifndef _I8085_STDLIB_H_
#define _I8085_STDLIB_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void abort(void) __attribute__((noreturn));
void exit(int status) __attribute__((noreturn));
void _Exit(int status) __attribute__((noreturn));
int atexit(void (*func)(void));

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _I8085_STDLIB_H_
