/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 *
 * Semantic analysis module.
 */
#ifndef IFJ26C_SEMANTICS_H
#define IFJ26C_SEMANTICS_H

#include "as_tree.h"

typedef enum {
  AERR_OK = 0,
  AERR_MEM,
  AERR_SEM,
} AError_t;

/* AnalyzeAST()
 *   Analyzes the whole AST.
 *
 * Returns:
 *   AERR_OK   all is fine and dandy
 *   AERR_MEM  need more ram (or other memory issue)
 *   AERR_SEM  the tree is semantically wrong
 */
AError_t AnalyzeAST(ASTNode_t *root);

#endif /* IFJ26C_SEMANTICS_H */
