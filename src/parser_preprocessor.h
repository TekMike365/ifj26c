/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#ifndef IFJ26C_PARSER_PREPROCESSOR_H
#define IFJ26C_PARSER_PREPROCESSOR_H

#include "token.h"

typedef struct preprocessor Preprocessor_t;

/* PPCreate()
 *   Heap allocated, needs to be deleted when done using it. Returns NULL
 * on error.
 */
Preprocessor_t *PPCreate(void);

/* PPCreate()
 *   Frees resources used by `p' and deletes it. Use after delete results in
 * undefined behavior;
 */
void PPDelete(Preprocessor_t *p);

/* PP_NextToken()
 *   Gets next token from Lexer's output stream applying a few transformations
 * listed below. Other than these transformations, its output is identical to
 * Lexer's GetNextToken().
 *
 *  1) Transforming TOKEN_PLUS and TOKEN_MINUS to TOKEN_UNARY_PLUS and
 * TOKEN_UNARY_MINUS based on the previous token respectively.
 *
 *  2) Handling brackets which cross multiple lines. Their indentation is
 * handeled automatically and no INDENT/DEDENT/NEWLINE tokens will be produced.
 * The optional trailing comma before the closing bracket will be also hidden.
 */
Token_t PPNextToken(Preprocessor_t *p);

#endif /* IFJ26C_PARSER_PREPROCESSOR_H */
