/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "char_buff.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

void CharBuffInit(CharBuff_t *buff) {
  assert(buff != NULL);

  buff->size = 0;
  buff->cpcity = 0;
  buff->buff = NULL;
}

bool CharBuffInsert(CharBuff_t *buff, char c) {
  assert(buff != NULL);

  if (buff->size == buff->cpcity) {
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

void CharBuffTerminate(CharBuff_t *buff) {
  if (!buff) {
    return;
  }

  free(buff->buff);
  buff->size = 0;
  buff->cpcity = 0;
}
