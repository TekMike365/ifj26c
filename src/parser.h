/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#ifndef IFJ26C_PARSER_INTERNAL_H
#define IFJ26C_PARSER_INTERNAL_H

#include "as_tree.h"
#include "parser_preprocessor.h"
#include <string.h>

// parser structure
// stores current token and next token
typedef struct Parser_t {
  SourceSpan_t lastValidPos;
  Token_t current;
  Token_t next;
  Token_t last;
  Preprocessor_t *pp;
  ErrorCode_t errorCode;
} Parser_t;


/* returns token type in string format, for error handling */
const char *TokenToString(TokenType_t type);

/* reports syntax error with detail */
void SyntaxError(Parser_t *p, const char *detail);

/* allocates memory and duplicates string */
char *StrDup(const char *s);

/* loads new token from preprocesor
 * If new p->current is ERROR_TOKEN, sets error code from TOKEN
 * returns true if no error, else false
 */
bool Advance(Parser_t *p);


ASTNode_t *Parse(ErrorCode_t *err);

ASTNode_t *ParseExpression(Parser_t *p);

#endif // IFJ26C_PARSER_INTERNAL_H