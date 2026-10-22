/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "char_buf.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

// initialize buffer
void CharBufInit(CharBuf_t *buf) {
  assert(buf != NULL);

  buf->size = 0;
  buf->capacity = 0;
  buf->buf = NULL;
}

// insert char c into buffer
bool CharBufInsert(CharBuf_t *buf, char c) {
  assert(buf != NULL);

  if (buf->size == buf->capacity) {
    // if the capacity is 0 (initialized), start with 8
    // else double the size
    size_t newSize = buf->capacity == 0 ? CHAR_BUF_MIN_CAP : buf->capacity * 2;

    char *tmp = realloc(buf->buf, newSize * sizeof(char));

    if (!tmp) {
      free(tmp);
      return false;
    }

    buf->capacity = newSize;
    buf->buf = tmp;
  }

  buf->buf[buf->size++] = c;
  return true;
}

// clear the buffer size for reuse
// without reallocating space
void CharBufClear(CharBuf_t *buf) {
  if (buf) {
    buf->size = 0;
  }
}

/* USE ONLY AFTER WRITING FINISHED
 * shrinks the buffer to buf->size if:
 * * buf->capacity > CHAR_BUF_MIN_CAP
 * * buf->capacity is greater than buf->size
 * * by CHAR_BUF_SHRINK_FACTOR
 *
 * used in a situation where the buffer is to be reused,
 * but previous stored data was much bigger in size
 * than the new one just stored
 */
void CharBufTryShrink(CharBuf_t *buf) {
  if (!buf)
    return;

  // if minimum capacity and sufficient shrink factor
  if ((buf->capacity >= CHAR_BUF_MIN_CAP) &&
      (buf->size * CHAR_BUF_SHRINK_FACTOR >= buf->capacity)) {
    // new capacity will be the current size
    size_t newCap = buf->size;

    char *tmp = realloc(buf->buf, newCap * sizeof(char));
    if (!tmp) {
      free(tmp);
      return;
    }

    buf->buf = tmp;
    buf->capacity = newCap;
  }
}

// destroy the buffer and free memory
void CharBufTerminate(CharBuf_t *buf) {
  if (!buf) {
    return;
  }

  free(buf->buf);
  buf->size = 0;
  buf->capacity = 0;
}
