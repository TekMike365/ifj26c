/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#include "bin_tree.h"
#include "logger.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NO_NODE SIZE_MAX

typedef enum BtColor { BT_BLACK, BT_RED } BtColor_t;
typedef enum BtDirection { BT_LEFT, BT_RIGHT } BtDirection_t;

struct SymbolTreeNode {
  SymbolHeader_t header;
  BtColor_t color;
  size_t left;
  size_t right;
  size_t parent;
};

/* Absent children use an index that can never identify a live array element. */
static void InsertFixup(SymbolTree_t *tree, size_t node);
static void DeleteFixup(SymbolTree_t *tree, size_t node, size_t parent,
                        int nodeIsLeft);
static void DeleteAt(SymbolTree_t *tree, size_t node);
static size_t MinimumNode(const SymbolTree_t *tree, size_t node);
static BtColor_t ColorOf(const SymbolTree_t *tree, size_t node);
static void SetColor(SymbolTree_t *tree, size_t node, BtColor_t color);
static void DestroyData(const SymbolTree_t *tree, void *data);
static void Rotate(SymbolTree_t *tree, size_t node,
                   BtDirection_t direction);
#ifdef DEBUG
static void SymbolTreeDebugPrintNode(const SymbolTree_t *tree, size_t node,
                                     const char *prefix, size_t prefixLength,
                                     bool isLast, bool isRoot);
#endif

bool SymbolTreeInit(SymbolTree_t *tree) {
  return SymbolTreeInitWithDestructor(tree, NULL);
}

bool SymbolTreeInitWithDestructor(SymbolTree_t *tree,
                                  SymbolDataDestroyFn_t destroyData) {
  if (tree == NULL) {
    return false;
  }

  tree->items = NULL;
  tree->size = 0;
  tree->capacity = 0;
  tree->root = NO_NODE;
  tree->destroyData = destroyData;
  return true;
}

void SymbolTreeDestroy(SymbolTree_t *tree) {
  if (tree == NULL) {
    return;
  }

  for (size_t index = 0; index < tree->size; index++) {
    DestroyData(tree, tree->items[index].header.data);
    free(tree->items[index].header.identifier);
  }
  free(tree->items);
  tree->items = NULL;
  tree->size = 0;
  tree->capacity = 0;
  tree->root = NO_NODE;
  tree->destroyData = NULL;
}

static size_t FindNode(const SymbolTree_t *tree, const char *identifier) {
  if (tree == NULL || identifier == NULL) {
    return NO_NODE;
  }

  size_t node = tree->root;
  while (node != NO_NODE && node < tree->size) {
    int comparison =
        strcmp(identifier, tree->items[node].header.identifier);
    if (comparison == 0) {
      return node;
    }
    node = comparison < 0 ? tree->items[node].left : tree->items[node].right;
  }
  return NO_NODE;
}

const SymbolHeader_t *SymbolTreeSearch(const SymbolTree_t *tree,
                                       const char *identifier) {
  size_t node = FindNode(tree, identifier);
  return node == NO_NODE ? NULL : &tree->items[node].header;
}

SymbolHeader_t *SymbolTreeSearchMutable(SymbolTree_t *tree,
                                        const char *identifier) {
  size_t node = FindNode(tree, identifier);
  return node == NO_NODE ? NULL : &tree->items[node].header;
}

#ifdef DEBUG
void SymbolTreeDebugPrint(const SymbolTree_t *tree) {
  if (tree == NULL || tree->root == NO_NODE) {
    DLOG_TRACE("(empty tree)");
    return;
  }

  SymbolTreeDebugPrintNode(tree, tree->root, "", 0, true, true);
}

static void SymbolTreeDebugPrintNode(const SymbolTree_t *tree, size_t node,
                                     const char *prefix, size_t prefixLength,
                                     bool isLast, bool isRoot) {
  if (node >= tree->size) {
    return;
  }

  const SymbolTreeNode_t *symbol = &tree->items[node];
  const char *color = symbol->color == BT_RED ? "RED" : "BLACK";
  if (isRoot) {
    DLOG_TRACE("%s [%s]", symbol->header.identifier, color);
  } else {
    DLOG_TRACE("%s%s%s [%s]", prefix, isLast ? "\\-- " : "|-- ",
               symbol->header.identifier, color);
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
    SymbolTreeDebugPrintNode(tree, symbol->left, childPrefix,
                             childPrefixLength, symbol->right == NO_NODE,
                             false);
  }
  if (symbol->right != NO_NODE) {
    SymbolTreeDebugPrintNode(tree, symbol->right, childPrefix,
                             childPrefixLength, true, false);
  }
}
#endif

const SymbolHeader_t *SymbolTreeAt(const SymbolTree_t *tree, size_t index) {
  if (tree == NULL || index >= tree->size) {
    return NULL;
  }

  return &tree->items[index].header;
}

bool SymbolTreeInsert(SymbolTree_t *tree, const char *identifier) {
  return SymbolTreeInsertWithData(tree, identifier, NULL);
}

bool SymbolTreeInsertWithData(SymbolTree_t *tree, const char *identifier,
                              void *data) {
  if (tree == NULL || identifier == NULL) {
    return false;
  }

  size_t newCapacity = tree->capacity;
  if (tree->size == tree->capacity) {
    if (newCapacity == 0) {
      newCapacity = 8;
    } else {
      if (newCapacity > SIZE_MAX / 2) {
        return false;
      }
      newCapacity *= 2;
    }
    if (newCapacity > SIZE_MAX / sizeof *tree->items) {
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

  if (newCapacity != tree->capacity) {
    SymbolTreeNode_t *newItems =
        realloc(tree->items, newCapacity * sizeof *tree->items);
    if (newItems == NULL) {
      free(identifierCopy);
      return false;
    }
    tree->items = newItems;
    tree->capacity = newCapacity;
  }

  size_t node = tree->size;
  SymbolTreeNode_t *symbol = &tree->items[node];
  symbol->header.identifier = identifierCopy;
  symbol->header.data = data;
  symbol->color = BT_RED;
  symbol->left = NO_NODE;
  symbol->right = NO_NODE;
  symbol->parent = NO_NODE;

  if (tree->root == NO_NODE) {
    tree->root = node;
  } else {
    size_t parent = tree->root;
    for (;;) {
      int comparison =
          strcmp(identifier, tree->items[parent].header.identifier);
      if (comparison < 0) {
        if (tree->items[parent].left == NO_NODE) {
          tree->items[parent].left = node;
          symbol->parent = parent;
          break;
        }
        parent = tree->items[parent].left;
      } else {
        if (tree->items[parent].right == NO_NODE) {
          tree->items[parent].right = node;
          symbol->parent = parent;
          break;
        }
        parent = tree->items[parent].right;
      }
    }
  }

  tree->size++;
  InsertFixup(tree, node);
  return true;
}

bool SymbolTreeSetDataAt(SymbolTree_t *tree, size_t index, void *data) {
  if (tree == NULL || index >= tree->size) {
    return false;
  }

  if (tree->items[index].header.data != data) {
    DestroyData(tree, tree->items[index].header.data);
    tree->items[index].header.data = data;
  }
  return true;
}

static inline bool SymbolTreeRemoveLast(SymbolTree_t *tree) {
  if (tree == NULL || tree->size == 0) {
    return false;
  }

  DeleteAt(tree, tree->size - 1);
  return true;
}

static void Rotate(SymbolTree_t *tree, size_t node,
                   BtDirection_t direction) {
  if (tree == NULL || node >= tree->size) {
    return;
  }

  size_t pivot;
  if (direction == BT_LEFT) {
    pivot = tree->items[node].right;
    if (pivot == NO_NODE) {
      return;
    }
    tree->items[node].right = tree->items[pivot].left;
    if (tree->items[node].right != NO_NODE) {
      tree->items[tree->items[node].right].parent = node;
    }
    tree->items[pivot].left = node;
  } else if (direction == BT_RIGHT) {
    pivot = tree->items[node].left;
    if (pivot == NO_NODE) {
      return;
    }
    tree->items[node].left = tree->items[pivot].right;
    if (tree->items[node].left != NO_NODE) {
      tree->items[tree->items[node].left].parent = node;
    }
    tree->items[pivot].right = node;
  } else {
    return;
  }

  size_t parent = tree->items[node].parent;
  tree->items[pivot].parent = parent;
  if (parent == NO_NODE) {
    tree->root = pivot;
  } else if (node == tree->items[parent].left) {
    tree->items[parent].left = pivot;
  } else {
    tree->items[parent].right = pivot;
  }
  tree->items[node].parent = pivot;
}

static BtColor_t ColorOf(const SymbolTree_t *tree, size_t node) {
  /* Missing children are the black NIL leaves used by red-black algorithms. */
  return node == NO_NODE || node >= tree->size ? BT_BLACK
                                               : tree->items[node].color;
}

static void SetColor(SymbolTree_t *tree, size_t node, BtColor_t color) {
  /* NIL leaves have no array slot and therefore cannot be recolored. */
  if (node != NO_NODE && node < tree->size) {
    tree->items[node].color = color;
  }
}

static void DestroyData(const SymbolTree_t *tree, void *data) {
  if (data != NULL && tree->destroyData != NULL) {
    tree->destroyData(data);
  }
}

/* Restore red-black invariants after insertion, mirroring cases by tree side. */
static void InsertFixup(SymbolTree_t *tree, size_t node) {
  while (node != tree->root &&
         ColorOf(tree, tree->items[node].parent) == BT_RED) {
    size_t parent = tree->items[node].parent;
    size_t grandparent = tree->items[parent].parent;

    if (parent == tree->items[grandparent].left) {
      size_t uncle = tree->items[grandparent].right;
      if (ColorOf(tree, uncle) == BT_RED) {
        /* Recolor and continue from the grandparent when the uncle is red. */
        SetColor(tree, parent, BT_BLACK);
        SetColor(tree, uncle, BT_BLACK);
        SetColor(tree, grandparent, BT_RED);
        node = grandparent;
      } else {
        /* A black uncle needs rotations to remove the red-parent violation. */
        if (node == tree->items[parent].right) {
          node = parent;
          Rotate(tree, node, BT_LEFT);
          parent = tree->items[node].parent;
          grandparent = tree->items[parent].parent;
        }
        SetColor(tree, parent, BT_BLACK);
        SetColor(tree, grandparent, BT_RED);
        Rotate(tree, grandparent, BT_RIGHT);
      }
    } else {
      size_t uncle = tree->items[grandparent].left;
      if (ColorOf(tree, uncle) == BT_RED) {
        SetColor(tree, parent, BT_BLACK);
        SetColor(tree, uncle, BT_BLACK);
        SetColor(tree, grandparent, BT_RED);
        node = grandparent;
      } else {
        if (node == tree->items[parent].left) {
          node = parent;
          Rotate(tree, node, BT_RIGHT);
          parent = tree->items[node].parent;
          grandparent = tree->items[parent].parent;
        }
        SetColor(tree, parent, BT_BLACK);
        SetColor(tree, grandparent, BT_RED);
        Rotate(tree, grandparent, BT_LEFT);
      }
    }
  }

  SetColor(tree, tree->root, BT_BLACK);
}

static void Transplant(SymbolTree_t *tree, size_t oldNode, size_t newNode) {
  /*
   * Replace oldNode in its parent's link (or the root). The caller must handle
   * oldNode's children separately if they need to be retained.
   */
  size_t parent = tree->items[oldNode].parent;
  if (parent == NO_NODE) {
    tree->root = newNode;
  } else if (oldNode == tree->items[parent].left) {
    tree->items[parent].left = newNode;
  } else {
    tree->items[parent].right = newNode;
  }

  if (newNode != NO_NODE) {
    tree->items[newNode].parent = parent;
  }
}

static size_t MinimumNode(const SymbolTree_t *tree, size_t node) {
  /* The leftmost node is the in-order successor candidate for deletion. */
  while (tree->items[node].left != NO_NODE) {
    node = tree->items[node].left;
  }
  return node;
}

static void DeleteAt(SymbolTree_t *tree, size_t node) {
  /*
   * For a two-child node, move its in-order successor into its place. The color
   * removed from its original position determines whether balancing is needed.
   */
  size_t moved = node;
  BtColor_t movedColor = tree->items[moved].color;
  size_t replacement;
  size_t replacementParent;
  int replacementIsLeft;

  if (tree->items[node].left == NO_NODE) {
    replacement = tree->items[node].right;
    replacementParent = tree->items[node].parent;
    replacementIsLeft = replacementParent != NO_NODE &&
                        node == tree->items[replacementParent].left;
    Transplant(tree, node, replacement);
  } else if (tree->items[node].right == NO_NODE) {
    replacement = tree->items[node].left;
    replacementParent = tree->items[node].parent;
    replacementIsLeft = replacementParent != NO_NODE &&
                        node == tree->items[replacementParent].left;
    Transplant(tree, node, replacement);
  } else {
    moved = MinimumNode(tree, tree->items[node].right);
    movedColor = tree->items[moved].color;
    replacement = tree->items[moved].right;
    if (tree->items[moved].parent != node) {
      replacementParent = tree->items[moved].parent;
      replacementIsLeft = moved == tree->items[replacementParent].left;
      Transplant(tree, moved, replacement);
      tree->items[moved].right = tree->items[node].right;
      tree->items[tree->items[moved].right].parent = moved;
    } else {
      replacementParent = moved;
      replacementIsLeft = 0;
      if (replacement != NO_NODE) {
        tree->items[replacement].parent = moved;
      }
    }
    Transplant(tree, node, moved);
    tree->items[moved].left = tree->items[node].left;
    tree->items[tree->items[moved].left].parent = moved;
    tree->items[moved].color = tree->items[node].color;
  }

  DestroyData(tree, tree->items[node].header.data);
  free(tree->items[node].header.identifier);
  if (movedColor == BT_BLACK) {
    DeleteFixup(tree, replacement, replacementParent, replacementIsLeft);
  }
  tree->size--;
  SetColor(tree, tree->root, BT_BLACK);
}

static void DeleteFixup(SymbolTree_t *tree, size_t node, size_t parent,
                        int nodeIsLeft) {
  /*
   * A missing replacement has no node from which to find its parent or side,
   * so those are carried explicitly. The two branches handle mirrored cases.
   */
  while (node != tree->root && ColorOf(tree, node) == BT_BLACK) {
    if (parent == NO_NODE) {
      break;
    }
    if (node != NO_NODE) {
      nodeIsLeft = node == tree->items[parent].left;
    }

    if (nodeIsLeft) {
      size_t sibling = tree->items[parent].right;
      if (ColorOf(tree, sibling) == BT_RED) {
        /* Convert to a black-sibling case before inspecting its children. */
        SetColor(tree, sibling, BT_BLACK);
        SetColor(tree, parent, BT_RED);
        Rotate(tree, parent, BT_LEFT);
        sibling = tree->items[parent].right;
      }

      size_t siblingLeft =
          sibling == NO_NODE ? NO_NODE : tree->items[sibling].left;
      size_t siblingRight =
          sibling == NO_NODE ? NO_NODE : tree->items[sibling].right;
      if (ColorOf(tree, siblingLeft) == BT_BLACK &&
          ColorOf(tree, siblingRight) == BT_BLACK) {
        /* Push the missing black upward when the sibling has no red child. */
        SetColor(tree, sibling, BT_RED);
        node = parent;
        parent = tree->items[node].parent;
      } else {
        if (ColorOf(tree, siblingRight) == BT_BLACK) {
          /* Rotate the near-red-child case into the far-child form. */
          SetColor(tree, siblingLeft, BT_BLACK);
          SetColor(tree, sibling, BT_RED);
          Rotate(tree, sibling, BT_RIGHT);
          sibling = tree->items[parent].right;
        }
        /* The far child can absorb the missing black after this rotation. */
        SetColor(tree, sibling, ColorOf(tree, parent));
        SetColor(tree, parent, BT_BLACK);
        if (sibling != NO_NODE) {
          SetColor(tree, tree->items[sibling].right, BT_BLACK);
        }
        Rotate(tree, parent, BT_LEFT);
        node = tree->root;
        parent = NO_NODE;
      }
    } else {
      size_t sibling = tree->items[parent].left;
      if (ColorOf(tree, sibling) == BT_RED) {
        SetColor(tree, sibling, BT_BLACK);
        SetColor(tree, parent, BT_RED);
        Rotate(tree, parent, BT_RIGHT);
        sibling = tree->items[parent].left;
      }

      size_t siblingRight =
          sibling == NO_NODE ? NO_NODE : tree->items[sibling].right;
      size_t siblingLeft =
          sibling == NO_NODE ? NO_NODE : tree->items[sibling].left;
      if (ColorOf(tree, siblingRight) == BT_BLACK &&
          ColorOf(tree, siblingLeft) == BT_BLACK) {
        SetColor(tree, sibling, BT_RED);
        node = parent;
        parent = tree->items[node].parent;
      } else {
        if (ColorOf(tree, siblingLeft) == BT_BLACK) {
          SetColor(tree, siblingRight, BT_BLACK);
          SetColor(tree, sibling, BT_RED);
          Rotate(tree, sibling, BT_LEFT);
          sibling = tree->items[parent].left;
        }
        SetColor(tree, sibling, ColorOf(tree, parent));
        SetColor(tree, parent, BT_BLACK);
        if (sibling != NO_NODE) {
          SetColor(tree, tree->items[sibling].left, BT_BLACK);
        }
        Rotate(tree, parent, BT_RIGHT);
        node = tree->root;
        parent = NO_NODE;
      }
    }
  }

  SetColor(tree, node, BT_BLACK);
}
