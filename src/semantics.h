/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 *
 * Semantic analysis module.
 */
#ifndef IFJ26C_SEMANTICS_H
#define IFJ26C_SEMANTICS_H

#include "as_tree.h"
#include <stdbool.h>

typedef enum {
  AERR_OK = 0,
  AERR_MEM,
  AERR_SEM,
} AError_t;

typedef struct analyzer Analyzer_t;

/* CreateAnalyzer()
 *   Creates new `Analyzer_t' instance or NULL if the creation fails.
 */
Analyzer_t *CreateAnalyzer(void);

/* CreateAnalyzer()
 *   Frees allocated resources used by the analyzer. Using analyzer after it
 * was deleted results in undefined behavior.
 */
void DeleteAnalyzer(Analyzer_t *a);

/* A_StartFunction()
 *   Start a new function definition and clear the block stack. If you wish to
 * check function body use the method for analyzing blocks or use
 * `A_AnalyzeStatement()' if the function doesn't have block body.
 *
 * PARAMETERS:
 *   fndef  function definition tree (without it's block, that is handled via
 *          the `A_BeginBlock' function)
 *
 * RETURN VALUE:
 *   AERR_OK   on success
 *   AERR_MEM  on memory allocation failure
 *   AERR_SEM  if `stmnt' is semantically incorrect
 */
AError_t A_StartFunction(Analyzer_t *a, const ASTNode_t *fndef);

/* A_BeginBlock()
 *   Begins a new block nested under the previous one. The root block is linked
 * to the last function added.
 *
 * RETURN VALUE:
 *   AERR_OK   on success
 *   AERR_MEM  on memory allocation failure
 */
AError_t A_BeginBlock(Analyzer_t *a);

/* A_EndBlock()
 *   Ends the last open block. If no block has started, it does nothing.
 */
void A_EndBlock(Analyzer_t *a);

/* A_AnalyzeStatement()
 *   Analyzes the passed in statement tree. If the statement requires a body
 * the body won't be analyzed. If you wish to analyze the body as well you
 * need to break it up into multiple `A_' function calls.
 *
 *   If an unsupported `stmnt' kind is passed in, it does nothing. Accepted
 * `stmnt' node kinds are:
 *    NODE_ASSIGN
 *    NODE_VARDEF
 *    NODE_VALDEF
 *    NODE_IF
 *    NODE_WHILE
 *    NODE_RETURN
 *
 * RETURN VALUE:
 *   AERR_OK   on success
 *   AERR_MEM  on memory allocation failure
 *   AERR_SEM  if `stmnt' is semantically incorrect
 */
AError_t A_AnalyzeStatement(Analyzer_t *a, ASTNode_t *stmnt);

#endif /* IFJ26C_SEMANTICS_H */
