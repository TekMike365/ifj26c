/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"

/**TODO:
 * e, E, d, D need separate type (letter v exponent/double)
 * change in state transitions therefore needed
 * transition table init to STATE_END
 * next state after STATE_START to STATE_ERROR, then overwrite
 * next state after STATE_COMMENT is COMMENT except upon CHAR_QUOTE
 * STATE_COMMENT needs separate handling
 */

// ASCII chars grouped into types for mapping
#define X_CHAR_TYPE(X)                                                         \
  X(CHAR_ALPHA)                                                                \
  X(CHAR_NUM)                                                                  \
  X(CHAR_EQ)                                                                   \
  X(CHAR_GREATER)                                                              \
  X(CHAR_LESS)                                                                 \
  X(CHAR_LPAR)                                                                 \
  X(CHAR_RPAR)                                                                 \
  X(CHAR_PLUS)                                                                 \
  X(CHAR_MINUS)                                                                \
  X(CHAR_STAR)                                                                 \
  X(CHAR_SLASH)                                                                \
  X(CHAR_DOT)                                                                  \
  X(CHAR_UNDERSCORE)                                                           \
  X(CHAR_QUOTE)                                                                \
  X(CHAR_WHITESPACE)                                                           \
  X(CHAR_NULL)                                                                 \
  X(CHAR_NEWLINE)                                                              \
  X(CHAR_OTHER)                                                                \
  X(CHAR_TYPE_CNT)

#define X(type) type,
typedef enum { X_CHAR_TYPE(X) } CharType_t;
#undef X

#define ASCII_LENGTH 256

/* specifies a range of chars of type 'type'
 * i.e. start='a', end='z' specifies CHAR_ALPHA
 */
typedef struct {
  unsigned char start;
  unsigned char end;
  CharType_t type;
} CharRange_t;

/** global map which maps char to CharType_t
 *  i.e. m_ASCIIMap[char c] = CharType_t of c
 */
/* not used yet - error
static CharType_t m_ASCIIMap[ASCII_LENGTH];


// populates the ASCIIMap with corresponding types
static void ASCIIMapInit(void) {
  // intially CHAR_OTHER
  for (int idx = 0; idx < ASCII_LENGTH; idx++) {
    m_ASCIIMap[idx] = CHAR_OTHER;
  }

  // define types for ranges
  CharRange_t ranges[] = {
      {'a', 'z', CHAR_ALPHA},        {'A', 'z', CHAR_ALPHA},
      {'0', '9', CHAR_NUM},          {'=', '=', CHAR_EQ},
      {'>', '>', CHAR_GREATER},      {'<', '<', CHAR_LESS},
      {'.', '.', CHAR_DOT},          {'_', '_', CHAR_UNDERSCORE},
      {'"', '"', CHAR_QUOTE},        {'(', '(', CHAR_LPAR},
      {')', ')', CHAR_RPAR},         {'+', '+', CHAR_PLUS},
      {'-', '-', CHAR_MINUS},        {'*', '*', CHAR_STAR},
      {'/', '/', CHAR_SLASH},        {'\0', '\0', CHAR_NULL},
      {'\n', '\n', CHAR_NEWLINE},    {' ', ' ', CHAR_WHITESPACE},
      {'\t', '\t', CHAR_WHITESPACE}, {'\v', '\v', CHAR_WHITESPACE},
      {'\r', '\r', CHAR_WHITESPACE}};

  // for each range
  for (int typeCnt = 0; typeCnt < CHAR_TYPE_CNT; typeCnt++) {
    // for each char in that range
    for (unsigned char idx = ranges[typeCnt].start; idx <= ranges[typeCnt].end;
         idx++) {
      // overwrite CHAR_OTHER with the specified type
      m_ASCIIMap[idx] = ranges[typeCnt].type;
    }
  }
}
*/

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
  X(STATE_LPAR)                                                                \
  X(STATE_RPAR)                                                                \
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
  X(STATE_END_STRING)                                                          \
  X(STATE_CNT)

#define X(state) state,
typedef enum { X_STATE(X) } State_t;
#undef X

// FSM transitions: (current, char type, next)
#define X_TRANSITION(X)                                                        \
  X(STATE_START, CHAR_ALPHA, STATE_IDENTIFIER)                                 \
  X(STATE_START, CHAR_LPAR, STATE_LPAR)                                        \
  X(STATE_START, CHAR_RPAR, STATE_RPAR)                                        \
  X(STATE_START, CHAR_EQ, STATE_EQ)                                            \
  X(STATE_START, CHAR_GREATER, STATE_GREATER)                                  \
  X(STATE_START, CHAR_LESS, STATE_LESS)                                        \
  X(STATE_START, CHAR_STAR, STATE_STAR)                                        \
  X(STATE_START, CHAR_PLUS, STATE_PLUS)                                        \
  X(STATE_START, CHAR_MINUS, STATE_MINUS)                                      \
  X(STATE_START, CHAR_SLASH, STATE_SLASH)                                      \
  X(STATE_START, CHAR_NUM, STATE_INT)                                          \
  X(STATE_START, CHAR_QUOTE, STATE_QUOTE)                                      \
  X(STATE_START, CHAR_DOT, STATE_DOT)                                          \
  X(STATE_IDENTIFIER, CHAR_ALPHA, STATE_IDENTIFIER)                            \
  X(STATE_IDENTIFIER, CHAR_NUM, STATE_IDENTIFIER)                              \
  X(STATE_EQ, CHAR_EQ, STATE_DOUBLE_EQ)                                        \
  X(STATE_GREATER, CHAR_EQ, STATE_GREATER_EQ)                                  \
  X(STATE_LESS, CHAR_EQ, STATE_LESS_EQ)

/* so far not used - error
static State_t transitionTable[STATE_CNT][CHAR_TYPE_CNT] = {STATE_END};

static void transitionTableInit(void) {
  for (int idx = 0; idx < STATE_CNT; idx++) {
    for (int idy = 0; idy < CHAR_TYPE_CNT; idy++) {
      if (idx == STATE_START) {
        transitionTable[idx][idy] = STATE_ERROR;
      } else {
        transitionTable[idx][idy] = STATE_END;
      }
    }
  }

#define X(curr, ch, next) transitionTable[curr][ch] = next;
  X_TRANSITION(X)
#undef X
}
*/
