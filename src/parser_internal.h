#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "lexer.h"
#include <stdbool.h>

typedef struct Parser_t {
  Token_t current;
  Token_t next;
  LayoutFilter_t filter;
  ErrorCode_t errorCode;
} Parser_t;

bool Advance(Parser_t *p);           // used by the PSA, so it can't be static anymore
bool ParseExpression(Parser_t *p);   // defined in expression.c

#endif