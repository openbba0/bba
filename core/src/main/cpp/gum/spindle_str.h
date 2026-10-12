#ifndef SPINDLE_STR_H
#define SPINDLE_STR_H

#include <stddef.h>

/* src is plaintext XOR 0xA7, including the encoded NUL (0xA7).
 * Kept out of line so the compiler cannot fold the XOR back into .rodata. */
void sp_dec(char *dst, const unsigned char *src, size_t n);

#endif
