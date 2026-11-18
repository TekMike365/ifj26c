/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

// ###################################################
//                     DESCRIPTION
// ###################################################
/* Module directly implements the FSM, converting
 *  character stream into raw tokens
 */
// ###################################################

#ifndef LEXER_H
#define LEXER_H

#include "char_buf.h"
#include "error.h"
#include "token.h"
#include <stddef.h>
#include <stdio.h>

// lexer structure
typedef struct Lexer Lexer_t;

/* Function is the direct implementation of the lexer FSM
 * It handles:
 * mapping ASCII char -> CHAR_TYPE
 * mapping the tuple (CHAR_TYPE, CURRENT STATE) ->  NEXT STATE
 * maps END STATE -> TOKEN_TYPE
 * looks up whether identifier is keyword
 * the token is "raw" in the sense that it outputs a token
 * irrespective of context (that is every NL, every indent, ...)
 * emits a special token TOKEN_EXPECT_EOL, used to validate whether
 * the last line of a block comment contains no token (except for newline)
 * note: the validation is not done here
 * converts int and double string literal to numerical value
 * saves identifiers and string literals
 * all other tokens are recognizable by their TokenType
 * */
Token_t GetRawToken(Lexer_t *lexer);

/* Initializes lexer
 * * src is the file to be read
 */
Lexer_t *LexerCreate(FILE *src);

// return lexer`s char buffer
CharBuf_t *LexerGetCharBuf(Lexer_t *lexer);

// return the underlying char buffer inside lexer`s CharBuf
char *LexerGetBuf(Lexer_t *lexer);

/* Terminate lexer */
void LexerTerminate(Lexer_t *lexer);

#endif /* ifndef LEXER_H */
