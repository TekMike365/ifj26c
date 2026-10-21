/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef CHAR_BUFF_H
#define CHAR_BUFF_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  size_t size;
  size_t cpcity;
  char *buff;
} CharBuff_t;

// TODO add threshold
// if buffer is reused and the
// new string is much smaller
// resize

void CharBuffInit(CharBuff_t *buff);

_Bool CharBuffInsert(CharBuff_t *buff, char c);

void CharBuffTerminate(CharBuff_t *buff);

#endif
