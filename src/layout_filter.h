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
typedef struct LayoutFilter LayoutFilter_t;

/* Initialize layout filter
 * * src - source file, passed to lexer
 * may fail on memory, because 0 is pushed on the indent stack
 * to act as a baseline
 * returns 0 on mem error, else 1
 * */
LayoutFilter_t *LayoutFilterCreate(FILE *src);

/* terminate layout filter */
void LayoutFilterTerminate(LayoutFilter_t *filter);

/* returns the next token */
Token_t GetNextToken(LayoutFilter_t *filter);

#endif
