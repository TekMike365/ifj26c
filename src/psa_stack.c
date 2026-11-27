/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */

#include "psa_stack.h"


/* Itiialises 
 * 
 */
bool PStackInit(PStack_t *stack) {
  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));

  if (newItem == NULL)
    return false;

  newItem->sym = PT_DOLLAR;
  newItem->node = NULL;
  newItem->next = NULL;
  stack->top = newItem;

  return true;
}

void PStackFree(PStack_t *stack) {
  if (stack == NULL)
    return;

  PStackItem_t *current = stack->top;
  while (current != NULL) {
    PStackItem_t *next = current->next;
    ASTFree(current->node);
    free(current);
    current = next;
  }
  stack->top = NULL;
}

// inserts handle after top-most terminal
bool PStackInsertHandle(PStack_t *stack) {
  if (stack == NULL || stack->top == NULL)
    return false;

  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));
  if (newItem == NULL)
    return false;

  newItem->sym = PT_HANDLE;
  newItem->node = NULL;
  // terminal is on top
  if (stack->top->sym <= PT_DOLLAR) {
    newItem->next = stack->top;
    stack->top = newItem;
    return true;
  }
  // terminal is deeper in stack
  PStackItem_t *current = stack->top;
  while (current->next != NULL && current->next->sym > PT_DOLLAR) {
    current = current->next;
  }

  if (current->next == NULL) {
    free(newItem);
    return false;
  }

  newItem->next = current->next;
  current->next = newItem;
  return true;
}

// If fails on memory allocation returns false, else true.
bool PStackPush(PStack_t *stack, PTSymbol_t sym, ASTNode_t *node) {
  if (stack == NULL)
    return false;

  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));
  if (newItem == NULL)
    return false;

  newItem->sym = sym;
  newItem->next = stack->top;
  newItem->node = node;
  stack->top = newItem;

  return true;
}

// returns the top-most terminal on the stack, return NULL if no terminal found
PStackItem_t *PStackTopTerminal(const PStack_t *stack) {
  if (stack == NULL)
    return NULL;

  PStackItem_t *current = stack->top;
  while (current != NULL && current->sym > PT_DOLLAR) {
    current = current->next;
  }
  return current;
}