/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "as_tree.h"

// parser structure
typedef struct Parser_t {
  Token_t current;
  Token_t next;
  LayoutFilter_t filter;
  ErrorCode_t errorCode;
} Parser_t;

void SetError(Parser_t *p);
bool Advance(Parser_t *p);
ASTNode_t *ParseExpression(Parser_t *p);

#endif