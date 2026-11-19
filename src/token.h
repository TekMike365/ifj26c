/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 *            Michael Sklenka <xsklenm00>
 */

// ###################################################
//                     DESCRIPTION
// ###################################################
/* Module defines the Token_t structure, token types
 * and operations on tokens
 */
// ###################################################

#ifndef TOKEN_H
#define TOKEN_H

#include "error.h"
#include <stddef.h>

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
  TOKEN_EXPECT_EOL, // internal token emitted after multiline block comment
  TOKEN_NOT_EQ,
  TOKEN_DOUBLE_EQ,
  TOKEN_GREATER,
  TOKEN_GREATER_EQ,
  TOKEN_LESS,
  TOKEN_LESS_EQ,
  TOKEN_LEFT_ARROW,
  TOKEN_STAR,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_SEMICOLON,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_ZERO,
  TOKEN_INT,
  TOKEN_DOUBLE,
  TOKEN_STRING,
  TOKEN_LPAR,
  TOKEN_RPAR,
  TOKEN_AT,
  TOKEN_AMPERSAND,
  TOKEN_PIPE,
  TOKEN_AND,
  TOKEN_OR,
  TOKEN_DOT,
  TOKEN_UNIT,
  TOKEN_EOF,
  TOKEN_NEWLINE,
  TOKEN_ERROR,

  /* `virtual' tokens */
  TOKEN_UNARY_PLUS,
  TOKEN_UNARY_MINUS,
} TokenType_t;

// stores the position of a token
typedef struct {
  size_t line;  // at what line
  size_t start; // line offset to start
  size_t end;   // line offset to end
} SourceSpan_t;

// info about indentation
typedef struct {
  int width;    // how many spaces
  _Bool hasTab; // was \t seen?
} IndentInfo_t;

// token structure
typedef struct {
  TokenType_t type; // token type
  SourceSpan_t pos; // position
  // attributes
  union {
    int lexemeI;         // integer lexeme
    double lexemeD;      // double lexeme
    char *lexemeS;       // string literal lexeme
    IndentInfo_t indent; // used by TOKEN_NEWLINE
    ErrorCode_t error;   // error code
  };
} Token_t;

/* creates a nulled token of type TOKEN_NONE*/
Token_t CreateToken(void);

/* terminates a token
 * if it is of type TOKEN_IDENTIFIER or TOKEN_STRING
 * and lexemeS is not NULL, frees it
 */
void TerminateToken(Token_t *token);

// returns a token of the specified type
Token_t MakeToken(TokenType_t type);

/* Sets a token to TOKEN_ERROR with the corresponding error code
 * * error - error code (MEM/LEX)
 * * token - token to be set
 */
void TokenSetError(ErrorCode_t error, Token_t *token);

/* Allocates memory for lexemeS and copies src into it
 * * token - token whose lexemeS is to be set
 * * src - source string
 * * length - length of the string
 * returns 0 on malloc failure, else 1
 */
_Bool TokenSetLexemeS(Token_t *token, const char *src, size_t length);

#endif
