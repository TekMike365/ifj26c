/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef LEXER_H
#define LEXER_H

#include "char_buf.h"
#include "int_stack.h"
#include <stddef.h>
#include <stdio.h>

typedef enum {
  TOKEN_NONE, // initial
  TOKEN_IDENTIFIER,
  TOKEN_INDENT,
  TOKEN_DEDENT,
  TOKEN_KEYWORD_DO,
  TOKEN_KEYWORD_IF,
  TOKEN_KEYWORD_DEF,
  TOKEN_KEYWORD_FOR,
  TOKEN_KEYWORD_INT,
  TOKEN_KEYWORD_VAL,
  TOKEN_KEYWORD_VAR,
  TOKEN_KEYWORD_CASE,
  TOKEN_KEYWORD_ELSE,
  TOKEN_KEYWORD_LIST,
  TOKEN_KEYWORD_THEN,
  TOKEN_KEYWORD_TYPE,
  TOKEN_KEYWORD_UNIT,
  TOKEN_KEYWORD_CLASS,
  TOKEN_KEYWORD_MATCH,
  TOKEN_KEYWORD_WHILE,
  TOKEN_KEYWORD_DOUBLE,
  TOKEN_KEYWORD_IMPORT,
  TOKEN_KEYWORD_RETURN,
  TOKEN_KEYWORD_STRING,
  TOKEN_KEYWORD_BOOLEAN,
  TOKEN_KEYWORD_EXTENSION,
  TOKEN_OPERATOR,
  TOKEN_SEMICOLON,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_STRING,
  TOKEN_LPAR,
  TOKEN_RPAR,
  TOKEN_AT,
  TOKEN_DOT,
  TOKEN_UNIT,
  TOKEN_EOF,
  TOKEN_NEWLINE,
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

  // stores active indent levels
  IntStack_t indentStack;
  // "queue" for TOKEN_INDENT
  long indentChange;
  // stores the previous character
  // used to replace ungetc
  // next invalid char is delimiter to token
  int prevChar;
} Lexer_t;

void LexerInit(Lexer_t *lexer, FILE *src);

void LexerTerminate(Lexer_t *lexer);

Token_t CreateToken(void);

Token_t GetNextToken(Lexer_t *lexer);

#endif /* ifndef LEXER_H */
