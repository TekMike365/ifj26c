/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"

// ASCII chars grouped into types for mapping
#define X_CHAR_TYPE(X)                                                         \
  X(CHAR_ALPHA)                                                                \
  X(CHAR_NUM)                                                                  \
  X(CHAR_EQ)                                                                   \
  X(CHAR_LPAR)                                                                 \
  X(CHAR_RPAR)                                                                 \
  X(CHAR_OP)                                                                   \
  X(CHAR_REL)                                                                  \
  X(CHAR_DOT)                                                                  \
  X(CHAR_QUOTE)                                                                \
  X(CHAR_SIGN)                                                                 \
  X(CHAR_WHITESPACE)                                                           \
  X(CHAR_NULL)                                                                 \
  X(CHAR_NEWLINE)                                                              \
  X(CHAR_UNKNWON)

// FSM states
// MISSING: MULTILINE STRING, COMMENT not complete
/** renamed (diagram -> macro):
 * Comment/Divide -> STATE_SLASH
 * Dot/Double -> STATE_DOT
 * Int/Double -> STATE_INT
 * String/Multiline s. -> STATE_QUOTE
 *
 */
#define X_STATE(X)                                                             \
  X(STATE_END)                                                                 \
  X(STATE_ERROR)                                                               \
  X(STATE_START)                                                               \
  X(STATE_IDENTIFIER)                                                          \
  X(STATE_LPAREN)                                                              \
  X(STATE_RPAREN)                                                              \
  X(STATE_EQ)                                                                  \
  X(STATE_DOUBLE_EQ)                                                           \
  X(STATE_GREATER)                                                             \
  X(STATE_GREATER_EQ)                                                          \
  X(STATE_LESS)                                                                \
  X(STATE_LESS_EQ)                                                             \
  X(STATE_STAR)                                                                \
  X(STATE_PLUS)                                                                \
  X(STATE_MINUS)                                                               \
  X(STATE_SLASH)                                                               \
  X(STATE_COMMENT)                                                             \
  X(STATE_BLOCK_COMMENT)                                                       \
  X(STATE_DOT)                                                                 \
  X(STATE_INT)                                                                 \
  X(STATE_INT_UNDERSCORE)                                                      \
  X(STATE_START_EXPONENT)                                                      \
  X(STATE_SIGN)                                                                \
  X(STATE_EXPONENT)                                                            \
  X(STATE_EXPONENT_UNDERSCORE)                                                 \
  X(STATE_START_DOUBLE)                                                        \
  X(STATE_DOUBLE)                                                              \
  X(STATE_DOUBLE_UNDERSCORE)                                                   \
  X(STATE_END_DOUBLE)                                                          \
  X(STATE_QUOTE)                                                               \
  X(STATE_ESCAPE)                                                              \
  X(STATE_STRING)                                                              \
  X(STATE_END_STRING)

#define X(type) type,
typedef enum { X_CHAR_TYPE(X) } CharType_t;
#undef X

#define X(state) state,
typedef enum { X_STATE(X) } State_t;
#undef X
