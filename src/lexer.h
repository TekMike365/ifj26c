/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef LEXER_H
#define LEXER_H

#include "char_buf.h"
#include <stddef.h>
#include <stdio.h>

typedef enum {
  TOKEN_NONE, // initial
  TOKEN_IDENTIFIER,
  TOKEN_KEYWORD,
  TOKEN_OPERATOR,
  TOKEN_SEMICOLON,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_STRING,
  TOKEN_LPAR,
  TOKEN_RPAR,
  TOKEN_DOT,
  TOKEN_UNIT,
  TOKEN_ERROR

} TokenType_t;

typedef struct {
  union {
    int lexemeI;
    double lexemeD;
    char *lexemeS;
  };
  size_t len;
  size_t line;
  TokenType_t type;
} Token_t;

typedef struct {
  FILE *src; // source file
  CharBuf_t buf;
  size_t offset; // file offset
  size_t line;   // counts newlines
} Lexer_t;

void LexerInit(Lexer_t *lexer, FILE *src);

void LexerTerminate(Lexer_t *lexer);

Token_t CreateToken(void);

Token_t NextToken(Lexer_t *lexer);

#endif /* ifndef LEXER_H */
