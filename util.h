/* util.h - a header for various utility functions */
#ifndef UTIL_H
#define UTIL_H
#include <stddef.h>
#include <sys/types.h>

extern void *emalloc(size_t);
extern void *erealloc(void *, size_t);
extern void efree(void **);
extern unsigned little_endian_8(u_int8_t *);
extern unsigned little_endian_16(u_int8_t *);
extern unsigned long little_endian_32(u_int8_t *);
extern unsigned long big_endian_32(u_int8_t *);

#endif  /* UTIL_H */


