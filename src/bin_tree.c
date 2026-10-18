/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#include "bin_tree.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_NODE SIZE_MAX

static void insertFixup(SymbolStack *stack, size_t node);
static void deleteFixup(SymbolStack *stack, size_t node, size_t parent,
                        int nodeIsLeft);
static void deleteAt(SymbolStack *stack, size_t node);
static size_t minimumNode(const SymbolStack *stack, size_t node);
static Color colorOf(const SymbolStack *stack, size_t node);
static void setColor(SymbolStack *stack, size_t node, Color color);
static void drawNode(const SymbolStack *stack, size_t node, const char *prefix,
                     size_t prefixLength, int isLast, int isRoot);

bool symbolStackInit(SymbolStack *stack) {
    if (stack == NULL) {
        return false;
    }

    stack->items = NULL;
    stack->size = 0;
    stack->capacity = 0;
    stack->root = NO_NODE;
    return true;
}

void symbolStackDestroy(SymbolStack *stack) {
    if (stack == NULL) {
        return;
    }

    for (size_t index = 0; index < stack->size; index++) {
        free(stack->items[index].identifier);
    }
    free(stack->items);
    stack->items = NULL;
    stack->size = 0;
    stack->capacity = 0;
    stack->root = NO_NODE;
}

const Symbol *symbolStackSearch(const SymbolStack *stack,
                                const char *identifier) {
    if (stack == NULL || identifier == NULL) {
        return NULL;
    }

    size_t node = stack->root;
    while (node != NO_NODE && node < stack->size) {
        int comparison = strcmp(identifier, stack->items[node].identifier);
        if (comparison == 0) {
            return &stack->items[node];
        }
        node =
            comparison < 0 ? stack->items[node].left : stack->items[node].right;
    }
    return NULL;
}

bool symbolStackPush(SymbolStack *stack, const char *identifier) {
    if (stack == NULL || identifier == NULL ||
        symbolStackSearch(stack, identifier) != NULL) {
        return false;
    }

    size_t newCapacity = stack->capacity;
    if (stack->size == stack->capacity) {
        if (newCapacity == 0) {
            newCapacity = 8;
        } else {
            if (newCapacity > SIZE_MAX / 2) {
                return false;
            }
            newCapacity *= 2;
        }
        if (newCapacity > SIZE_MAX / sizeof *stack->items) {
            return false;
        }
    }

    size_t identifierLength = strlen(identifier);
    if (identifierLength == SIZE_MAX) {
        return false;
    }
    char *identifierCopy = malloc(identifierLength + 1);
    if (identifierCopy == NULL) {
        return false;
    }
    memcpy(identifierCopy, identifier, identifierLength + 1);

    if (newCapacity != stack->capacity) {
        Symbol *newItems =
            realloc(stack->items, newCapacity * sizeof *stack->items);
        if (newItems == NULL) {
            free(identifierCopy);
            return false;
        }
        stack->items = newItems;
        stack->capacity = newCapacity;
    }

    size_t node = stack->size;
    Symbol *symbol = &stack->items[node];
    symbol->identifier = identifierCopy;
    symbol->color = RED;
    symbol->left = NO_NODE;
    symbol->right = NO_NODE;
    symbol->parent = NO_NODE;

    if (stack->root == NO_NODE) {
        stack->root = node;
    } else {
        size_t parent = stack->root;
        for (;;) {
            int comparison =
                strcmp(identifier, stack->items[parent].identifier);
            if (comparison < 0) {
                if (stack->items[parent].left == NO_NODE) {
                    stack->items[parent].left = node;
                    symbol->parent = parent;
                    break;
                }
                parent = stack->items[parent].left;
            } else {
                if (stack->items[parent].right == NO_NODE) {
                    stack->items[parent].right = node;
                    symbol->parent = parent;
                    break;
                }
                parent = stack->items[parent].right;
            }
        }
    }

    stack->size++;
    insertFixup(stack, node);
    return true;
}

bool symbolStackPop(SymbolStack *stack) {
    if (stack == NULL || stack->size == 0) {
        return false;
    }

    deleteAt(stack, stack->size - 1);
    return true;
}

void rotate(SymbolStack *stack, size_t node, Direction direction) {
    if (stack == NULL || node >= stack->size) {
        return;
    }

    size_t pivot;
    if (direction == LEFT) {
        pivot = stack->items[node].right;
        if (pivot == NO_NODE) {
            return;
        }
        stack->items[node].right = stack->items[pivot].left;
        if (stack->items[node].right != NO_NODE) {
            stack->items[stack->items[node].right].parent = node;
        }
        stack->items[pivot].left = node;
    } else if (direction == RIGHT) {
        pivot = stack->items[node].left;
        if (pivot == NO_NODE) {
            return;
        }
        stack->items[node].left = stack->items[pivot].right;
        if (stack->items[node].left != NO_NODE) {
            stack->items[stack->items[node].left].parent = node;
        }
        stack->items[pivot].right = node;
    } else {
        return;
    }

    size_t parent = stack->items[node].parent;
    stack->items[pivot].parent = parent;
    if (parent == NO_NODE) {
        stack->root = pivot;
    } else if (node == stack->items[parent].left) {
        stack->items[parent].left = pivot;
    } else {
        stack->items[parent].right = pivot;
    }
    stack->items[node].parent = pivot;
}

void draw(const SymbolStack *stack) {
    if (stack == NULL || stack->root == NO_NODE) {
        puts("(empty tree)");
        return;
    }

    drawNode(stack, stack->root, "", 0, 1, 1);
}

static void drawNode(const SymbolStack *stack, size_t node, const char *prefix,
                     size_t prefixLength, int isLast, int isRoot) {
    const Symbol *symbol = &stack->items[node];
    const char *color = symbol->color == RED ? "RED" : "BLACK";
    if (isRoot) {
        printf("%s [%s]\n", symbol->identifier, color);
    } else {
        printf("%s%s%s [%s]\n", prefix, isLast ? "\\-- " : "|-- ",
               symbol->identifier, color);
    }

    size_t childPrefixLength = prefixLength;
    char childPrefix[prefixLength + 5];
    if (prefixLength > 0) {
        memcpy(childPrefix, prefix, prefixLength);
    }
    if (isRoot) {
        childPrefix[0] = '\0';
        childPrefixLength = 0;
    } else {
        memcpy(childPrefix + prefixLength, isLast ? "    " : "|   ", 5);
        childPrefixLength += 4;
    }

    if (symbol->left != NO_NODE) {
        drawNode(stack, symbol->left, childPrefix, childPrefixLength,
                 symbol->right == NO_NODE, 0);
    }
    if (symbol->right != NO_NODE) {
        drawNode(stack, symbol->right, childPrefix, childPrefixLength, 1, 0);
    }
}

static Color colorOf(const SymbolStack *stack, size_t node) {
    return node == NO_NODE || node >= stack->size ? BLACK
                                                  : stack->items[node].color;
}

static void setColor(SymbolStack *stack, size_t node, Color color) {
    if (node != NO_NODE && node < stack->size) {
        stack->items[node].color = color;
    }
}

static void insertFixup(SymbolStack *stack, size_t node) {
    while (node != stack->root &&
           colorOf(stack, stack->items[node].parent) == RED) {
        size_t parent = stack->items[node].parent;
        size_t grandparent = stack->items[parent].parent;

        if (parent == stack->items[grandparent].left) {
            size_t uncle = stack->items[grandparent].right;
            if (colorOf(stack, uncle) == RED) {
                setColor(stack, parent, BLACK);
                setColor(stack, uncle, BLACK);
                setColor(stack, grandparent, RED);
                node = grandparent;
            } else {
                if (node == stack->items[parent].right) {
                    node = parent;
                    rotate(stack, node, LEFT);
                    parent = stack->items[node].parent;
                    grandparent = stack->items[parent].parent;
                }
                setColor(stack, parent, BLACK);
                setColor(stack, grandparent, RED);
                rotate(stack, grandparent, RIGHT);
            }
        } else {
            size_t uncle = stack->items[grandparent].left;
            if (colorOf(stack, uncle) == RED) {
                setColor(stack, parent, BLACK);
                setColor(stack, uncle, BLACK);
                setColor(stack, grandparent, RED);
                node = grandparent;
            } else {
                if (node == stack->items[parent].left) {
                    node = parent;
                    rotate(stack, node, RIGHT);
                    parent = stack->items[node].parent;
                    grandparent = stack->items[parent].parent;
                }
                setColor(stack, parent, BLACK);
                setColor(stack, grandparent, RED);
                rotate(stack, grandparent, LEFT);
            }
        }
    }

    setColor(stack, stack->root, BLACK);
}

static void transplant(SymbolStack *stack, size_t oldNode, size_t newNode) {
    size_t parent = stack->items[oldNode].parent;
    if (parent == NO_NODE) {
        stack->root = newNode;
    } else if (oldNode == stack->items[parent].left) {
        stack->items[parent].left = newNode;
    } else {
        stack->items[parent].right = newNode;
    }

    if (newNode != NO_NODE) {
        stack->items[newNode].parent = parent;
    }
}

static size_t minimumNode(const SymbolStack *stack, size_t node) {
    while (stack->items[node].left != NO_NODE) {
        node = stack->items[node].left;
    }
    return node;
}

static void deleteAt(SymbolStack *stack, size_t node) {
    size_t moved = node;
    Color movedColor = stack->items[moved].color;
    size_t replacement;
    size_t replacementParent;
    int replacementIsLeft;

    if (stack->items[node].left == NO_NODE) {
        replacement = stack->items[node].right;
        replacementParent = stack->items[node].parent;
        replacementIsLeft = replacementParent != NO_NODE &&
                            node == stack->items[replacementParent].left;
        transplant(stack, node, replacement);
    } else if (stack->items[node].right == NO_NODE) {
        replacement = stack->items[node].left;
        replacementParent = stack->items[node].parent;
        replacementIsLeft = replacementParent != NO_NODE &&
                            node == stack->items[replacementParent].left;
        transplant(stack, node, replacement);
    } else {
        moved = minimumNode(stack, stack->items[node].right);
        movedColor = stack->items[moved].color;
        replacement = stack->items[moved].right;
        if (stack->items[moved].parent != node) {
            replacementParent = stack->items[moved].parent;
            replacementIsLeft = moved == stack->items[replacementParent].left;
            transplant(stack, moved, replacement);
            stack->items[moved].right = stack->items[node].right;
            stack->items[stack->items[moved].right].parent = moved;
        } else {
            replacementParent = moved;
            replacementIsLeft = 0;
            if (replacement != NO_NODE) {
                stack->items[replacement].parent = moved;
            }
        }
        transplant(stack, node, moved);
        stack->items[moved].left = stack->items[node].left;
        stack->items[stack->items[moved].left].parent = moved;
        stack->items[moved].color = stack->items[node].color;
    }

    free(stack->items[node].identifier);
    if (movedColor == BLACK) {
        deleteFixup(stack, replacement, replacementParent, replacementIsLeft);
    }
    stack->size--;
    setColor(stack, stack->root, BLACK);
}

static void deleteFixup(SymbolStack *stack, size_t node, size_t parent,
                        int nodeIsLeft) {
    while (node != stack->root && colorOf(stack, node) == BLACK) {
        if (parent == NO_NODE) {
            break;
        }
        if (node != NO_NODE) {
            nodeIsLeft = node == stack->items[parent].left;
        }

        if (nodeIsLeft) {
            size_t sibling = stack->items[parent].right;
            if (colorOf(stack, sibling) == RED) {
                setColor(stack, sibling, BLACK);
                setColor(stack, parent, RED);
                rotate(stack, parent, LEFT);
                sibling = stack->items[parent].right;
            }

            size_t siblingLeft =
                sibling == NO_NODE ? NO_NODE : stack->items[sibling].left;
            size_t siblingRight =
                sibling == NO_NODE ? NO_NODE : stack->items[sibling].right;
            if (colorOf(stack, siblingLeft) == BLACK &&
                colorOf(stack, siblingRight) == BLACK) {
                setColor(stack, sibling, RED);
                node = parent;
                parent = stack->items[node].parent;
            } else {
                if (colorOf(stack, siblingRight) == BLACK) {
                    setColor(stack, siblingLeft, BLACK);
                    setColor(stack, sibling, RED);
                    rotate(stack, sibling, RIGHT);
                    sibling = stack->items[parent].right;
                }
                setColor(stack, sibling, colorOf(stack, parent));
                setColor(stack, parent, BLACK);
                if (sibling != NO_NODE) {
                    setColor(stack, stack->items[sibling].right, BLACK);
                }
                rotate(stack, parent, LEFT);
                node = stack->root;
                parent = NO_NODE;
            }
        } else {
            size_t sibling = stack->items[parent].left;
            if (colorOf(stack, sibling) == RED) {
                setColor(stack, sibling, BLACK);
                setColor(stack, parent, RED);
                rotate(stack, parent, RIGHT);
                sibling = stack->items[parent].left;
            }

            size_t siblingRight =
                sibling == NO_NODE ? NO_NODE : stack->items[sibling].right;
            size_t siblingLeft =
                sibling == NO_NODE ? NO_NODE : stack->items[sibling].left;
            if (colorOf(stack, siblingRight) == BLACK &&
                colorOf(stack, siblingLeft) == BLACK) {
                setColor(stack, sibling, RED);
                node = parent;
                parent = stack->items[node].parent;
            } else {
                if (colorOf(stack, siblingLeft) == BLACK) {
                    setColor(stack, siblingRight, BLACK);
                    setColor(stack, sibling, RED);
                    rotate(stack, sibling, LEFT);
                    sibling = stack->items[parent].left;
                }
                setColor(stack, sibling, colorOf(stack, parent));
                setColor(stack, parent, BLACK);
                if (sibling != NO_NODE) {
                    setColor(stack, stack->items[sibling].left, BLACK);
                }
                rotate(stack, parent, RIGHT);
                node = stack->root;
                parent = NO_NODE;
            }
        }
    }

    setColor(stack, node, BLACK);
}
