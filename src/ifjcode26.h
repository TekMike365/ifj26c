/* Implementace imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00> 
 */

#ifndef IFJ26C_IFJCODE26_H
#define IFJ26C_IFJCODE26_H

#include "as_tree.h"

/**
 * Generate IFJcode26 from a semantically checked AST.
 *
 * The AST must be a NODE_PROGRAM containing function definitions, including
 * exactly one zero-argument entry function marked with fnDef.isMain. The
 * generated program is written to output. Returns false for an unsupported or
 * malformed AST, or when writing to output fails; output may be partial.
 */
bool IFJCode26Generate(const ASTNode_t *program, FILE *output);

#endif
