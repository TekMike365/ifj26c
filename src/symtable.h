/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>

/** Callback used to release values owned by the table. */
typedef void (*SymbolDataDestroyFn_t)(void *data);

/** Opaque symbol table; its data structure is private to symtable.c. */
typedef struct SymbolTable SymbolTable_t;

/** A caller-owned list node returned by SymbolTableGetAll. */
typedef struct SymbolTableValueNode {
  void *value;
  struct SymbolTableValueNode *next;
} SymbolTableValueNode_t;

/**
 * Create an empty symbol table.
 *
 * Values inserted successfully are owned by the table and released with
 * destroyData. Pass NULL when values do not need destruction.
 *
 * @return the table, or NULL if allocation fails.
 */
SymbolTable_t *SymbolTableCreate(SymbolDataDestroyFn_t destroyData);

/** Free the table, its copied identifiers, and its owned values. */
void SymbolTableDestroy(SymbolTable_t *table);

/**
 * Insert a symbol using its identifier as the key.
 *
 * On success, the table takes ownership of data. If the identifier already
 * exists, another entry with the same identifier is added. On failure,
 * ownership remains with the caller.
 */
bool SymbolTableInsert(SymbolTable_t *table, const char *identifier,
                       void *data);

/**
 * Look up one symbol by identifier. If duplicates exist, which match is
 * returned is unspecified.
 *
 * Returns true when found and writes its value to data. A stored NULL value is
 * distinguishable from a missing identifier through the return value.
 */
bool SymbolTableGet(const SymbolTable_t *table, const char *identifier,
                    void **data);

/**
 * Get all values associated with an identifier.
 *
 * The returned list is allocated by this function and is in reverse insertion
 * order. The caller must free it with SymbolTableValueListDestroy; the values
 * themselves remain owned by the table. On failure, *values is set to NULL.
 */
bool SymbolTableGetAll(const SymbolTable_t *table, const char *identifier,
                       SymbolTableValueNode_t **values);

/** Free a list returned by SymbolTableGetAll without freeing its values. */
void SymbolTableValueListDestroy(SymbolTableValueNode_t *values);

/**
 * Replace the value for one matching identifier.
 *
 * The old value is released using the table's destructor. On success, the
 * table takes ownership of data; otherwise ownership remains with the caller.
 * If the identifier occurs more than once, one matching entry is replaced.
 */
bool SymbolTableReplace(SymbolTable_t *table, const char *identifier,
                        void *data);

/** Write a diagnostic visualization through the debug logger. */
void SymbolTableDebugPrint(const SymbolTable_t *table);

#endif
