/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef INT_STACK_H
#define INT_STACK_H

#include <stddef.h>

#define INT_STACK_MIN_CAP 8

typedef struct {
  size_t top;
  size_t capacity;
  int *data;
} IntStack_t;

void IntStackInit(IntStack_t *stack);

_Bool IntStackPush(IntStack_t *stack, int val);

void IntStackPop(IntStack_t *stack);

int IntStackTop(IntStack_t *stack);

_Bool IntStackIsEmpty(IntStack_t *stack);

void IntStackTerminate(IntStack_t *stack);

_Bool IntStackContains(IntStack_t *stack, int val);

#endif
