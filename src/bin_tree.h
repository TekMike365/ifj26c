/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#ifndef BIN_TREE_H
#define BIN_TREE_H

#include <stdbool.h>
#include <stddef.h>

/** Semantic symbol data, independent of the tree's balancing metadata. */
typedef struct SymbolHeader {
  /** Owned, null-terminated copy of the symbol's identifier. */
  char *identifier;
  /** Optional user-owned payload; ownership transfers to the tree on insert. */
  void *data;
} SymbolHeader_t;

/** Opaque internal node type used by SymbolTree_t. */
typedef struct SymbolTreeNode SymbolTreeNode_t;

/** Function used to release a symbol's optional payload. */
typedef void (*SymbolDataDestroyFn_t)(void *data);

/**
 * Red-black search tree backed by a resizable array.
 *
 * Tree links and root are indices into items rather than pointers, so growing
 * the array does not invalidate links. The node implementation is private;
 * callers can work with SymbolHeader_t values returned by the tree API.
 */
typedef struct SymbolTree {
  /** Opaque resizable storage for all live nodes. */
  SymbolTreeNode_t *items;
  /** Number of live nodes in items. */
  size_t size;
  /** Number of allocated slots in items. */
  size_t capacity;
  /** Index of the root node, or SIZE_MAX when the tree is empty. */
  size_t root;
  /** Optional destructor for payloads owned by the tree. */
  SymbolDataDestroyFn_t destroyData;
} SymbolTree_t;

/**
 * Initialize an empty tree.
 *
 * @return true on success, or false if tree is NULL.
 */
bool SymbolTreeInit(SymbolTree_t *tree);

/**
 * Initialize an empty tree with a payload destructor.
 *
 * Payloads passed to SymbolTreeInsertWithData are released by this callback
 * when replaced, removed, or when the tree is destroyed. If no destructor is
 * needed, use SymbolTreeInit.
 */
bool SymbolTreeInitWithDestructor(SymbolTree_t *tree,
                                  SymbolDataDestroyFn_t destroyData);

/** Free all identifiers, payloads, and storage owned by the tree. */
void SymbolTreeDestroy(SymbolTree_t *tree);

/**
 * Insert a copy of identifier into the tree with no payload.
 *
 * Duplicate identifiers are allowed.
 *
 * @return true if inserted, or false for invalid input or allocation failure.
 */
bool SymbolTreeInsert(SymbolTree_t *tree, const char *identifier);

/**
 * Insert an identifier and payload into the tree.
 *
 * Duplicate identifiers are allowed. On success, the tree owns data and will
 * pass it to its configured destructor. On failure, ownership remains with
 * the caller.
 *
 * @return true if inserted, or false for invalid input or allocation failure.
 */
bool SymbolTreeInsertWithData(SymbolTree_t *tree, const char *identifier,
                              void *data);

/**
 * Replace the payload of the item at index.
 *
 * The old payload is released with the configured destructor. On success, the
 * tree takes ownership of data; on failure, ownership remains with the caller.
 */
bool SymbolTreeSetDataAt(SymbolTree_t *tree, size_t index, void *data);

/**
 * Get a symbol by its array index.
 *
 * @return the symbol, or NULL if tree is NULL or index is out of bounds.
 */
const SymbolHeader_t *SymbolTreeAt(const SymbolTree_t *tree, size_t index);

/**
 * Find a symbol by identifier.
 *
 * If multiple symbols have this identifier, one matching symbol is returned.
 * Use SymbolTreeAt to inspect every item.
 *
 * @return the symbol, or NULL if not found or an argument is NULL.
 */
const SymbolHeader_t *SymbolTreeSearch(const SymbolTree_t *tree,
                                       const char *identifier);

/**
 * Find a symbol by identifier for mutation.
 *
 * The identifier is a tree key and must not be changed. The payload may be
 * modified. Assigning a payload transfers ownership to the tree; use
 * SymbolTreeSetDataAt when replacing an existing payload so its destructor is
 * called.
 *
 * @return the symbol, or NULL if not found or an argument is NULL.
 */
SymbolHeader_t *SymbolTreeSearchMutable(SymbolTree_t *tree,
                                        const char *identifier);

#ifdef DEBUG
/**
 * Write a diagnostic visualization of the tree through the debug logger.
 *
 * This function is available only in DEBUG builds and writes to stderr, not
 * stdout.
 */
void SymbolTreeDebugPrint(const SymbolTree_t *tree);
#endif

#endif
