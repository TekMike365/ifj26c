/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "char_buff.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

// initialize buffer
void CharBuffInit(CharBuff_t *buff) {
  assert(buff != NULL);

  buff->size = 0;
  buff->cpcity = 0;
  buff->buff = NULL;
}

// insert char c into buffer
bool CharBuffInsert(CharBuff_t *buff, char c) {
  assert(buff != NULL);

  if (buff->size == buff->cpcity) {
    // if the capacity is 0 (initialized), start with 8
    // else double the size
    size_t newSize = buff->cpcity == 0 ? 8 : buff->cpcity * 2;

    char *tmp = realloc(buff->buff, newSize * sizeof(char));

    if (!tmp) {
      free(tmp);
      return false;
    }

    buff->cpcity = newSize;
    buff->buff = tmp;
  }

  buff->buff[buff->size++] = c;
  return true;
}

// clear the buffer size for reuse
// without reallocating space
void CharBuffClear(CharBuff_t *buff) {
  if (buff) {
    buff->size = 0;
  }
}

// destroy the buffer and free memory
void CharBuffTerminate(CharBuff_t *buff) {
  if (!buff) {
    return;
  }

  free(buff->buff);
  buff->size = 0;
  buff->cpcity = 0;
}
