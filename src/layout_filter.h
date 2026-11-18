/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

// ###################################################
//                     DESCRIPTION
// ###################################################
/* Module defines the Layout Filter, which is a layer
 * sitting above lexer but below parser. its jobs is
 * to pull raw tokens from the stream and use a set of
 * flags do decide which tokens to let through,
 * which to intercept and which to store for later
 * emission
 *
 * it handles correct newline emission, indentation,
 * etc.
 *
 * the most important takeaway is that to get the next
 * token, GetNextToken function should be called
 * (as opposed to GetRawToken defined in lexer.h,
 * which emits tokens without context)
 */
// ###################################################

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
