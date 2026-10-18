/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#ifndef BIN_TREE_H
#define BIN_TREE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum Color { BLACK, RED } Color;

typedef struct Symbol {
    char *identifier;
    Color color;
    size_t left;
    size_t right;
    size_t parent;
} Symbol;

typedef struct SymbolStack {
    Symbol *items;
    size_t size;
    size_t capacity;
    size_t root;
} SymbolStack;

typedef enum Direction { LEFT, RIGHT } Direction;

bool symbolStackInit(SymbolStack *stack);
void symbolStackDestroy(SymbolStack *stack);
bool symbolStackPush(SymbolStack *stack, const char *identifier);
bool symbolStackPop(SymbolStack *stack);
const Symbol *symbolStackSearch(const SymbolStack *stack,
                                const char *identifier);
void rotate(SymbolStack *stack, size_t node, Direction direction);
void draw(const SymbolStack *stack);

#endif