/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#ifndef IFJ26C_PSA_STACK_H
#define IFJ26C_PSA_STACK_H

#include "as_tree.h"
#include "psa_table.h"


// contains symbol used for PT
// if terminal or nonterminal, the node points to AS tree, else NULL
typedef struct PStackItem_t {
  ASTNode_t *node;
  PTSymbol_t sym;
  struct PStackItem_t *next;
} PStackItem_t;

// contains terminals nonterminals or handles plus AS trees.
typedef struct PStack_t {
  PStackItem_t *top;
} PStack_t;

 // inserts handle after top-most terminal
bool PStackInsertHandle(PStack_t *stack);

void PStackFree(PStack_t *stack);

bool PStackInit(PStack_t *stack);

// If fails on memory allocation returns false, else true.
bool PStackPush(PStack_t *stack, PTSymbol_t sym, ASTNode_t *node);

// returns the top-most terminal on the stack, return NULL if no terminal found
PStackItem_t *PStackTopTerminal(const PStack_t *stack);

#endif // IFJ26C_PSA_TABLE_H