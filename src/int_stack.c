/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "int_stack.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

struct IntStack {
  size_t top;
  size_t capacity;
  int *data;
};

// initialize stack
IntStack_t *IntStackCreate(void) {
  IntStack_t *stack = malloc(sizeof(*stack));
  if (!stack) {
    return NULL;
  }

  stack->data = NULL;
  // top points at the next free index
  stack->top = 0;
  stack->capacity = 0;

  return stack;
}

// push "val" on top of the stack
_Bool IntStackPush(IntStack_t *stack, int val) {
  if (!stack) {
    return 0;
  }
  if (stack->capacity == stack->top) {
    // if 0, set capactiy to minimum, else double
    size_t newCap =
        stack->capacity == 0 ? INT_STACK_MIN_CAP : stack->capacity * 2;

    int *tmp = realloc(stack->data, newCap * sizeof(int));
    if (!tmp) {
      return 0;
    }

    stack->data = tmp;
    stack->capacity = newCap;
  }

  // push value at the top and advance
  stack->data[stack->top++] = val;
  return 1;
}

// pop the top value
void IntStackPop(IntStack_t *stack) {
  if (!stack) {
    return;
  }

  if (stack->top > 0) {
    stack->top--;
  }
}

// return value at the top
int IntStackTop(IntStack_t *stack) {
  assert(stack != NULL);
  assert(!IntStackIsEmpty(stack));

  // top is the next free index
  // hence top value is at top - 1
  return stack->data[stack->top - 1];
}

_Bool IntStackIsEmpty(IntStack_t *stack) {
  if (!stack) {
    return 1;
  }
  return stack->top == 0;
}

void IntStackTerminate(IntStack_t *stack) {
  if (!stack)
    return;

  free(stack->data);
  free(stack);
}

_Bool IntStackContains(IntStack_t *stack, int val) {
  if (!stack) {
    return 0;
  }
  if (!stack->data) {
    return 0;
  }

  for (size_t idx = 0; idx < stack->top; idx++) {
    if (stack->data[idx] == val) {
      return 1;
    }
  }
  return 0;
}
