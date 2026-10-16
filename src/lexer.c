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
typedef enum {
  CHAR_ALPHA,
  CHAR_NUM,
  CHAR_EQ,
  CHAR_GREATER,
  CHAR_LESS,
  CHAR_LPAR,
  CHAR_RPAR,
  CHAR_PLUS,
  CHAR_MINUS,
  CHAR_STAR,
  CHAR_SLASH,
  CHAR_DOT,
  CHAR_UNDERSCORE,
  CHAR_QUOTE,
  CHAR_WHITESPACE,
  CHAR_NULL,
  CHAR_NEWLINE,
  CHAR_OTHER,
  CHAR_TYPE_CNT
} CharType_t;

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
 *  i.e. m_ASCIIMap[unsigned char c] = CharType_t of c
 */
static CharType_t m_ASCIIMap[ASCII_LENGTH];

// populates the ASCIIMap with corresponding types
static void ASCIIMapInit(void) {
  // intially CHAR_OTHER
  for (int idx = 0; idx < ASCII_LENGTH; idx++) {
    m_ASCIIMap[idx] = CHAR_OTHER;
  }

  // define types for ranges
  CharRange_t ranges[] = {
      {'a', 'z', CHAR_ALPHA},        {'A', 'Z', CHAR_ALPHA},
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

/*
static inline CharType_t MapChar(unsigned char c) { return m_ASCIIMap[c]; }
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
typedef enum {
  STATE_END,
  STATE_ERROR,
  STATE_START,
  STATE_IDENTIFIER,
  STATE_LPAR,
  STATE_RPAR,
  STATE_EQ,
  STATE_DOUBLE_EQ,
  STATE_GREATER,
  STATE_GREATER_EQ,
  STATE_LESS,
  STATE_LESS_EQ,
  STATE_STAR,
  STATE_PLUS,
  STATE_MINUS,
  STATE_SLASH,
  STATE_COMMENT,
  STATE_BLOCK_COMMENT,
  STATE_DOT,
  STATE_INT,
  STATE_INT_UNDERSCORE,
  STATE_START_EXPONENT,
  STATE_SIGN,
  STATE_EXPONENT,
  STATE_EXPONENT_UNDERSCORE,
  STATE_START_DOUBLE,
  STATE_DOUBLE,
  STATE_DOUBLE_UNDERSCORE,
  STATE_END_DOUBLE,
  STATE_QUOTE,
  STATE_ESCAPE,
  STATE_STRING,
  STATE_END_STRING,
  STATE_CNT
} State_t;

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

static State_t m_transitionTable[STATE_CNT][CHAR_TYPE_CNT] = {STATE_END};

static void TransitionTableInit(void) {
  // initially set all states to STATE_END
  for (int idx = 0; idx < STATE_CNT; idx++) {
    for (int idy = 0; idy < CHAR_TYPE_CNT; idy++) {
      // except for STATE_START
      // because unexpected character -> no known token start
      // -> error
      if (idx == STATE_START) {
        m_transitionTable[idx][idy] = STATE_ERROR;
      }
      // STATE_COMMENT
      // everything is a comment until CHAR_QUOTE
      else if (idx == STATE_COMMENT) {
        m_transitionTable[idx][idy] = STATE_COMMENT;
      } else {
        m_transitionTable[idx][idy] = STATE_END;
      }
    }
  }

#define X(curr, ch, next) m_transitionTable[curr][ch] = next;
  X_TRANSITION(X)
#undef X
}

/** initialize a lexer struct
 * lexer - pointer to a lexer struct
 * src - pointer to buffer to scan
 */
void LexerInit(Lexer_t *lexer, const char *src) {
  if (!lexer) {
    return;
  }
  lexer->source = src;
  lexer->offset = 0;
  lexer->line = 1; // first line

  // initialize the map and transition table
  ASCIIMapInit();
  TransitionTableInit();
}
