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
  size_t line;  // at what line
  size_t start; // line offset to start
  size_t end;   // line offset to end
} SourceSpan_t;

typedef struct {
  int width;
  _Bool hasTab;
} IndentInfo_t;

typedef enum { ERROR_LEX = 1, ERROR_MEM = 99, ERROR_NOERR = 0 } ErrorCode_t;

typedef struct {
  TokenType_t type;
  SourceSpan_t pos;
  union {
    int lexemeI;
    double lexemeD;
    char *lexemeS;
    IndentInfo_t indent; // used by TOKEN_NEWLINE
    ErrorCode_t error;
  };
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
  // "queue" for TOKEN_DEDENT
  // TOKEN_INDENT can be only one at a time,
  // so it gets emitted right away
  int dedentQueue;

  IndentInfo_t nextIndent;

  int bracketDepth;
} LayoutFilter_t;

void LexerInit(Lexer_t *lexer, FILE *src);

void LexerTerminate(Lexer_t *lexer);

_Bool LayoutFilterInit(LayoutFilter_t *filter, FILE *src);

void LayoutFilterTerminate(LayoutFilter_t *filter);

Token_t CreateToken(void);

void TerminateToken(Token_t *token);

Token_t GetNextToken(LayoutFilter_t *filter);

#endif /* ifndef LEXER_H */
