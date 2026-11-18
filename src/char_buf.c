/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "char_buf.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

struct CharBuf {
  size_t size;     // the next free slot, also size
  size_t capacity; // current capacity
  char *buf;
};

// create buffer
CharBuf_t *CharBufCreate() {
  CharBuf_t *buf = malloc(sizeof(*buf));
  if (!buf) {
    return NULL;
  }

  buf->size = 0;
  buf->capacity = 0;
  buf->buf = NULL;
  return buf;
}

// insert char c into buffer
_Bool CharBufInsert(CharBuf_t *buf, char c) {
  if (!buf) {
    return 0;
  }

  // +1 because of null
  if (buf->size + 1 >= buf->capacity) {
    // if the capacity is 0 (initialized), start with 8
    // else double the size
    size_t newSize = buf->capacity == 0 ? CHAR_BUF_MIN_CAP : buf->capacity * 2;

    char *tmp = realloc(buf->buf, newSize * sizeof(char));

    if (!tmp) {
      return 0;
    }

    buf->capacity = newSize;
    buf->buf = tmp;
  }

  buf->buf[buf->size++] = c;
  // null terminate
  buf->buf[buf->size] = '\0';
  return 1;
}

// clear the buffer size for reuse
// without reallocating space
// null-terminate to create empty string
void CharBufClear(CharBuf_t *buf) {
  if (buf) {
    buf->size = 0;
    if (buf->buf) {
      buf->buf[0] = '\0';
    }
  }
}

char *CharBufGetBuf(CharBuf_t *buf) {
  if (!buf) {
    return NULL;
  }
  return buf->buf;
}

size_t CharBufGetSize(CharBuf_t *buf) {
  if (!buf) {
    return 0;
  }

  return buf->size;
}

_Bool CharBufTryShrink(CharBuf_t *buf) {
  if (!buf)
    return 0;

  size_t requiredCap = buf->size + 1;
  size_t minCap =
      CHAR_BUF_MIN_CAP > requiredCap ? CHAR_BUF_MIN_CAP : requiredCap;

  // if minimum capacity and sufficient shrink factor
  if ((buf->capacity > minCap) &&
      (buf->size * CHAR_BUF_SHRINK_FACTOR < buf->capacity)) {
    // new capacity will be the current size unless smaller than min cap

    char *tmp = realloc(buf->buf, minCap * sizeof(char));
    if (!tmp) {
      return 0;
    }

    buf->buf = tmp;
    buf->capacity = minCap;
  }

  return 1;
}

// destroy the buffer and free memory
void CharBufTerminate(CharBuf_t *buf) {
  if (!buf) {
    return;
  }

  free(buf->buf);
  free(buf);
}
