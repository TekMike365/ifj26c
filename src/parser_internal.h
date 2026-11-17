#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "lexer.h"
#include <stdbool.h>

// parser structure
typedef struct Parser_t {
  Token_t current;
  Token_t next;
  LayoutFilter_t filter;
  ErrorCode_t errorCode;
} Parser_t;


bool Advance(Parser_t *p);
bool ParseExpression(Parser_t *p);

#endif