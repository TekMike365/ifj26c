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
  TOKEN_EQ,
  TOKEN_NOT_EQ,
  TOKEN_DOUBLE_EQ,
  TOKEN_GREATER,
  TOKEN_GREATER_EQ,
  TOKEN_LESS,
  TOKEN_LESS_EQ,
  TOKEN_STAR,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
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
    int indent; // used by TOKEN_NEWLINE
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

  // stores the previous character
  // used to replace ungetc
  // next invalid char is delimiter to token
  int prevChar;
} Lexer_t;

typedef struct {
  Lexer_t lexer;
  // indent stack
  IntStack_t indents;

  // storage for one token
  Token_t stash;
  // is something stored?
  _Bool hasStash;

  // line start?
  _Bool atLineStart;
  // number of TOKEN_DEDENTs to emit
  // TOKEN_INDENT can be only one
  int pendingDedents;

  // indent from last newline
  int nextIndent;
  // contains tab?
  _Bool nextTab;

  int bracketDepth;
} LayoutFilter_t;

void LexerInit(Lexer_t *lexer, FILE *src);

void LexerTerminate(Lexer_t *lexer);

_Bool LayoutFilterInit(LayoutFilter_t *filter, FILE *src);

void LayoutFilterTerminate(LayoutFilter_t *filter);

Token_t CreateToken(void);

Token_t GetNextToken(LayoutFilter_t *filter);

#endif /* ifndef LEXER_H */
