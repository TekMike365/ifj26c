/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 *
 * Implementation of the integer stack ADT
 * and the core operation on it
 */

#ifndef INT_STACK_H
#define INT_STACK_H

#include <stddef.h>

#define INT_STACK_MIN_CAP 8

typedef struct IntStack IntStack_t;

// return pointer to IntStack_t
IntStack_t *IntStackCreate(void);

// push a value on top of the stack
// return 1 on success, else 0 (stack is NULL or realloc failed)
_Bool IntStackPush(IntStack_t *stack, int val);

// pops the value at top off the stack
// noop if stack is NULL
void IntStackPop(IntStack_t *stack);

// returns the value at the top of the stack
// CHECK whether stack is not NULL (guarded by assert)
int IntStackTop(IntStack_t *stack);

// is the stack empty?
_Bool IntStackIsEmpty(IntStack_t *stack);

// does the stack contain the value val?
// enables to verify a value is stored without popping
_Bool IntStackContains(IntStack_t *stack, int val);

// destroy IntStack
void IntStackTerminate(IntStack_t *stack);

#endif
