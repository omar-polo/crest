#ifndef COMPAT_H
#define COMPAT_H

#include <sys/types.h>
#include <sys/uio.h>

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "queue.h"

typedef uint8_t u_char;

#ifndef __OpenBSD__
#define pledge(s, p) (0)
#endif

/* ugly, I know, but:
 * 1. we don't have many fd open
 * 2. detect the function presence require a more complex
 *    setup than just a plain Makefile.
 */
#ifndef __OpenBSD__
#define getdtablecount() (0)
#endif

void freezero(void*, size_t);
void *recallocarray(void*, size_t, size_t, size_t);

#endif

