/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
  TOKEN_IDENTIFIER,
  TOKEN_KEYWORD,
  TOKEN_OPERATOR

} TokenType_t;

typedef struct {
  const char *start;
  size_t len;
  size_t line;
  TokenType_t type;
} Token_t;

typedef struct {
  const char *source; // points at buffer start
  size_t offset;      // offset in that buffer
  size_t line;        // counts newlines
} Lexer_t;

void LexerInit(Lexer_t *lexer, const char *src);

Token_t NextToken(Lexer_t *lexer);

#endif /* ifndef LEXER_H */
