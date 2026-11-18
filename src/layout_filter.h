/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "int_stack.h"
#include "lexer.h"
#include "token.h"

#ifndef LAYOUT_FILTER_H
#define LAYOUT_FILTER_H

// layout filter struct
// encapsulates lexer, indent stack and flags
typedef struct {
  Lexer_t *lexer;
  // indent stack
  IntStack_t *indents;

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

  // needed to validate whether no token follows after
  // /* of the last line in block comment
  _Bool expectEOL;

  // info about current line`s indent
  IndentInfo_t nextIndent;

  // count of bracket nesting
  int bracketDepth;
} LayoutFilter_t;

/* Initialize layout filter
 * * src - source file, passed to lexer
 * may fail on memory, because 0 is pushed on the indent stack
 * to act as a baseline
 * returns 0 on mem error, else 1
 * */
_Bool LayoutFilterInit(LayoutFilter_t *filter, FILE *src);

/* terminate layout filter */
void LayoutFilterTerminate(LayoutFilter_t *filter);

/* returns the next token */
Token_t GetNextToken(LayoutFilter_t *filter);

#endif
