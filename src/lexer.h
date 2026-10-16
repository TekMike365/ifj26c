/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef LEXER_H
#define LEXER_H

typedef enum {
  TOKEN_IDENTIFIER,
  TOKEN_KEYWORD,
  TOKEN_OPERATOR

} TokenType_t;

typedef struct {
  const char *start;
  unsigned len;
  unsigned line;
  TokenType_t type;
} Token_t;

Token_t NextToken(void);

#endif /* ifndef LEXER_H */
