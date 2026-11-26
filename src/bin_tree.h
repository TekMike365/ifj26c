/* Implementace imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#ifndef BIN_TREE_H
#define BIN_TREE_H

#include "symtable.h"

#include <stddef.h>

/**
 * Compatibility view of a symbol's identifier and associated user data.
 *
 * Pointers returned by the tree API are borrowed. Do not modify identifier:
 * it is the search key. Adding entries may reallocate storage and invalidate
 * previously returned pointers.
 */
typedef struct SymbolHeader {
  char *identifier;
  void *data;
} SymbolHeader_t;

/**
 * Stack-allocatable handle for the legacy SymbolTree API.
 *
 * The implementation pointer is private to the module; initialize and destroy
 * the handle only with the functions below.
 */
typedef struct SymbolTree {
  SymbolTable_t *implementation;
} SymbolTree_t;

/**
 * One result from SymbolTreeFindAllIndices.
 *
 * The index refers to the symbol's insertion-order slot and can be passed to
 * SymbolTreeAt or SymbolTreeSetDataAt.
 */
typedef struct SymbolTreeIndexNode {
  size_t index;
  struct SymbolTreeIndexNode *next;
} SymbolTreeIndexNode_t;

/**
 * Initialize an empty tree without a payload destructor.
 *
 * The handle must be initialized before any other operation. Returns false if
 * tree is NULL or allocation fails.
 */
bool SymbolTreeInit(SymbolTree_t *tree);

/**
 * Initialize an empty tree with a destructor for stored payloads.
 *
 * The destructor is called for non-NULL payloads when replaced or when the
 * tree is destroyed. Returns false if tree is NULL or allocation fails.
 */
bool SymbolTreeInitWithDestructor(SymbolTree_t *tree,
                                  SymbolDataDestroyFn_t destroyData);

/** Destroy the tree and its owned identifiers and payloads. */
void SymbolTreeDestroy(SymbolTree_t *tree);

/**
 * Insert a copied identifier with a NULL payload.
 *
 * Duplicate identifiers are allowed. Returns false for invalid input or
 * allocation failure.
 */
bool SymbolTreeInsert(SymbolTree_t *tree, const char *identifier);

/**
 * Insert a copied identifier and payload.
 *
 * Duplicate identifiers are allowed. On success, the tree takes ownership of
 * data; on failure, ownership remains with the caller.
 */
bool SymbolTreeInsertWithData(SymbolTree_t *tree, const char *identifier,
                              void *data);

/**
 * Replace the payload at an insertion-order index.
 *
 * The old payload is released through the configured destructor. On success,
 * the tree takes ownership of data; on failure, ownership remains with the
 * caller.
 */
bool SymbolTreeSetDataAt(SymbolTree_t *tree, size_t index, void *data);

/**
 * Return the symbol at an insertion-order index, or NULL if out of range.
 *
 * The returned pointer is borrowed and may be invalidated by a later insert
 * that reallocates the internal storage.
 */
const SymbolHeader_t *SymbolTreeAt(const SymbolTree_t *tree, size_t index);

/**
 * Find one symbol with this identifier, or NULL if none exists.
 *
 * With duplicate identifiers, which matching symbol is returned is
 * unspecified. The result is borrowed and may be invalidated by a later
 * insert that reallocates internal storage.
 */
const SymbolHeader_t *SymbolTreeSearch(const SymbolTree_t *tree,
                                       const char *identifier);

/**
 * Find one symbol with this identifier for payload mutation.
 *
 * With duplicate identifiers, which matching symbol is returned is
 * unspecified. Do not modify its identifier. Use SymbolTreeSetDataAt to
 * replace its payload while ensuring the old payload's destructor is called.
 */
SymbolHeader_t *SymbolTreeSearchMutable(SymbolTree_t *tree,
                                        const char *identifier);

/**
 * Find the indexes of all symbols with this identifier.
 *
 * Returns true on success, including when there are no matches. The allocated
 * list is in reverse insertion order and must be released with
 * SymbolTreeIndexListDestroy. On failure, *indices is set to NULL.
 */
bool SymbolTreeFindAllIndices(const SymbolTree_t *tree,
                              const char *identifier,
                              SymbolTreeIndexNode_t **indices);

/** Free a list returned by SymbolTreeFindAllIndices. */
void SymbolTreeIndexListDestroy(SymbolTreeIndexNode_t *indices);

/**
 * Write a debug visualization through the debug logger.
 *
 * In debug builds, output goes to stderr; in release builds, this does
 * nothing.
 */
void SymbolTreeDebugPrint(const SymbolTree_t *tree);

#endif
