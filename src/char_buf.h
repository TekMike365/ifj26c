/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef CHAR_BUF_H
#define CHAR_BUF_H

#include <stddef.h>
// minimum buffer capacity
// of a non-empty buffer
#define CHAR_BUF_MIN_CAP 8

// buffer can shrink only
// if CPCITY/SIZE is this or more
#define CHAR_BUF_SHRINK_FACTOR 4

typedef struct {
  size_t size;
  size_t capacity;
  char *buf;
} CharBuf_t;

void CharBufInit(CharBuf_t *buf);

_Bool CharBufInsert(CharBuf_t *buf, char c);

void CharBufClear(CharBuf_t *buf);

/* USE ONLY AFTER WRITING FINISHED
 * shrinks the buffer to buf->size if:
 * * buf->capacity > CHAR_BUF_MIN_CAP
 * * buf->capacity is greater than buf->size
 * * by CHAR_BUF_SHRINK_FACTOR
 * returns 0 on realloc failure, else 1
 *
 * used in a situation where the buffer is to be reused,
 * but previous stored data was much bigger in size
 * than the new one just stored
 */
_Bool CharBufTryShrink(CharBuf_t *buf);

void CharBufTerminate(CharBuf_t *buf);

#endif
