/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef LEXER_FSM_H
#define LEXER_FSM_H

// ##############################################################
//                        ASCII MAP STUFF
// ##############################################################

#define ASCII_LENGTH 128

// ASCII chars grouped into types for mapping
typedef enum {
  CHAR_ALPHA,
  CHAR_D, // d and D in double
  CHAR_E, // e end E in exponent
  CHAR_N, // escape newline
  CHAR_R, // -- carriage return
  CHAR_T, // -- tabulator
  CHAR_NUM,
  CHAR_EQ,
  CHAR_GREATER,
  CHAR_LESS,
  CHAR_LPAR,
  CHAR_EXCL_MARK,
  CHAR_AT,
  CHAR_SEMICOLON,
  CHAR_COMMA,
  CHAR_COLON,
  CHAR_RPAR,
  CHAR_PLUS,
  CHAR_MINUS,
  CHAR_STAR,
  CHAR_SLASH,
  CHAR_BACKSLASH,
  CHAR_DOT,
  CHAR_UNDERSCORE,
  CHAR_QUOTE,
  CHAR_WHITESPACE,
  CHAR_NULL,
  CHAR_LINE_FEED,
  CHAR_OTHER,
  CHAR_TYPE_CNT
} CharType_t;

/* specifies a range of chars of type 'type'
 * i.e. start='a', end='z' specifies CHAR_ALPHA
 */
typedef struct {
  unsigned char start;
  unsigned char end;
  CharType_t type;
} CharRange_t;

/* maps char to CharType_t
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
  CharRange_t ranges[] = {{'a', 'z', CHAR_ALPHA},
                          {'A', 'Z', CHAR_ALPHA},
                          {'d', 'd', CHAR_D},
                          {'D', 'D', CHAR_D},
                          {'e', 'e', CHAR_E},
                          {'E', 'E', CHAR_E},
                          {'n', 'n', CHAR_N},
                          {'r', 'r', CHAR_R},
                          {'t', 't', CHAR_T},
                          {'0', '9', CHAR_NUM},
                          {'=', '=', CHAR_EQ},
                          {'>', '>', CHAR_GREATER},
                          {'<', '<', CHAR_LESS},
                          {'.', '.', CHAR_DOT},
                          {'_', '_', CHAR_UNDERSCORE},
                          {'"', '"', CHAR_QUOTE},
                          {'(', '(', CHAR_LPAR},
                          {')', ')', CHAR_RPAR},
                          {'!', '!', CHAR_EXCL_MARK},
                          {'@', '@', CHAR_AT},
                          {';', ';', CHAR_SEMICOLON},
                          {',', ',', CHAR_COMMA},
                          {':', ':', CHAR_COLON},
                          {'+', '+', CHAR_PLUS},
                          {'-', '-', CHAR_MINUS},
                          {'*', '*', CHAR_STAR},
                          {'/', '/', CHAR_SLASH},
                          {'\\', '\\', CHAR_BACKSLASH},
                          {'\0', '\0', CHAR_NULL},
                          {'\n', '\n', CHAR_LINE_FEED},
                          {' ', ' ', CHAR_WHITESPACE},
                          {'\t', '\t', CHAR_WHITESPACE},
                          {'\v', '\v', CHAR_WHITESPACE},
                          {'\r', '\r', CHAR_WHITESPACE}};

  size_t rangeSize = sizeof(ranges) / sizeof(ranges[0]);
  // for each range
  for (size_t rangeCnt = 0; rangeCnt < rangeSize; rangeCnt++) {
    // for each char in that range
    for (unsigned char idx = ranges[rangeCnt].start;
         idx <= ranges[rangeCnt].end; idx++) {
      // overwrite CHAR_OTHER with the specified type
      m_ASCIIMap[idx] = ranges[rangeCnt].type;
    }
  }
}

/* maps an unsigned char to its corresponding CharType via ASCIIMap
 * c - unsigned char
 * return - CharType of c
 */
static inline CharType_t MapChar(unsigned char c) { return m_ASCIIMap[c]; }

// ##############################################################
//                        FSM STUFF
// ##############################################################

// FSM states
typedef enum {
  STATE_END,
  STATE_ERROR,
  STATE_START,
  STATE_IDENTIFIER,
  STATE_LPAR,
  STATE_RPAR,
  STATE_EXCL_MARK,
  STATE_AT,
  STATE_NOT_EQ,
  STATE_UNIT,
  STATE_SEMICOLON,
  STATE_COMMA,
  STATE_COLON,
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
  STATE_DQUOTE, // double quote
  STATE_MULTILINE_STR,
  STATE_END_QUOTE,
  STATE_END_DQUOTE,
  STATE_END_MULTILINE_STR,
  STATE_NEWLINE,
  STATE_CNT
} State_t;

// abstraction to prevent typing these out repeatedly
#define ALPHA_GROUP(state_curr, state_next)                                    \
  X((state_curr), CHAR_ALPHA, (state_next))                                    \
  X((state_curr), CHAR_D, (state_next))                                        \
  X((state_curr), CHAR_E, (state_next))                                        \
  X((state_curr), CHAR_N, (state_next))                                        \
  X((state_curr), CHAR_R, (state_next))                                        \
  X((state_curr), CHAR_T, (state_next))

// FSM transitions: (current, char type, next)
#define X_TRANSITIONS(X)                                                       \
  ALPHA_GROUP(STATE_START, STATE_IDENTIFIER)                                   \
  X(STATE_START, CHAR_UNDERSCORE, STATE_IDENTIFIER)                            \
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
  X(STATE_START, CHAR_EXCL_MARK, STATE_EXCL_MARK)                              \
  X(STATE_START, CHAR_AT, STATE_AT)                                            \
  X(STATE_START, CHAR_SEMICOLON, STATE_SEMICOLON)                              \
  X(STATE_START, CHAR_SEMICOLON, STATE_COMMA)                                  \
  X(STATE_START, CHAR_SEMICOLON, STATE_COLON)                                  \
  X(STATE_START, CHAR_LINE_FEED, STATE_NEWLINE)                                \
                                                                               \
  ALPHA_GROUP(STATE_IDENTIFIER, STATE_IDENTIFIER)                              \
  X(STATE_IDENTIFIER, CHAR_UNDERSCORE, STATE_IDENTIFIER)                       \
  X(STATE_IDENTIFIER, CHAR_NUM, STATE_IDENTIFIER)                              \
                                                                               \
  X(STATE_LPAR, CHAR_RPAR, STATE_UNIT)                                         \
                                                                               \
  X(STATE_EQ, CHAR_EQ, STATE_DOUBLE_EQ)                                        \
  X(STATE_GREATER, CHAR_EQ, STATE_GREATER_EQ)                                  \
  X(STATE_LESS, CHAR_EQ, STATE_LESS_EQ)                                        \
                                                                               \
  X(STATE_EXCL_MARK, CHAR_EQ, STATE_NOT_EQ)                                    \
                                                                               \
  X(STATE_SLASH, CHAR_SLASH, STATE_COMMENT)                                    \
  X(STATE_SLASH, CHAR_STAR, STATE_BLOCK_COMMENT)                               \
                                                                               \
  X(STATE_INT, CHAR_NUM, STATE_INT)                                            \
  X(STATE_INT, CHAR_UNDERSCORE, STATE_INT_UNDERSCORE)                          \
  X(STATE_INT, CHAR_DOT, STATE_START_DOUBLE)                                   \
  X(STATE_INT, CHAR_E, STATE_START_EXPONENT)                                   \
  X(STATE_INT_UNDERSCORE, CHAR_NUM, STATE_INT)                                 \
  X(STATE_INT_UNDERSCORE, CHAR_UNDERSCORE, STATE_INT_UNDERSCORE)               \
                                                                               \
  X(STATE_DOT, CHAR_NUM, STATE_DOUBLE)                                         \
                                                                               \
  X(STATE_QUOTE, CHAR_QUOTE, STATE_DQUOTE)                                     \
  X(STATE_QUOTE, CHAR_BACKSLASH, STATE_ESCAPE)                                 \
                                                                               \
  X(STATE_STRING, CHAR_QUOTE, STATE_END_STRING)                                \
  X(STATE_STRING, CHAR_BACKSLASH, STATE_ESCAPE)                                \
                                                                               \
  X(STATE_ESCAPE, CHAR_BACKSLASH, STATE_STRING)                                \
  X(STATE_ESCAPE, CHAR_QUOTE, STATE_STRING)                                    \
  X(STATE_ESCAPE, CHAR_N, STATE_STRING)                                        \
  X(STATE_ESCAPE, CHAR_R, STATE_STRING)                                        \
  X(STATE_ESCAPE, CHAR_T, STATE_STRING)                                        \
                                                                               \
  X(STATE_START_EXPONENT, CHAR_PLUS, STATE_SIGN)                               \
  X(STATE_START_EXPONENT, CHAR_MINUS, STATE_SIGN)                              \
  X(STATE_START_EXPONENT, CHAR_NUM, STATE_EXPONENT)                            \
                                                                               \
  X(STATE_EXPONENT, CHAR_NUM, STATE_EXPONENT)                                  \
  X(STATE_EXPONENT, CHAR_UNDERSCORE, STATE_EXPONENT_UNDERSCORE)                \
  X(STATE_EXPONENT, CHAR_D, STATE_END_DOUBLE)                                  \
  X(STATE_EXPONENT_UNDERSCORE, CHAR_NUM, STATE_EXPONENT)                       \
  X(STATE_EXPONENT_UNDERSCORE, CHAR_UNDERSCORE, STATE_EXPONENT_UNDERSCORE)     \
                                                                               \
  X(STATE_DOUBLE, CHAR_NUM, STATE_DOUBLE)                                      \
  X(STATE_DOUBLE, CHAR_UNDERSCORE, STATE_DOUBLE_UNDERSCORE)                    \
  X(STATE_DOUBLE, CHAR_E, STATE_START_EXPONENT)                                \
  X(STATE_DOUBLE, CHAR_D, STATE_END_DOUBLE)                                    \
  X(STATE_DOUBLE_UNDERSCORE, CHAR_NUM, STATE_DOUBLE)                           \
  X(STATE_DOUBLE_UNDERSCORE, CHAR_UNDERSCORE, STATE_DOUBLE_UNDERSCORE)         \
                                                                               \
  X(STATE_DQUOTE, CHAR_QUOTE, STATE_MULTILINE_STR)                             \
  X(STATE_MULTILINE_STR, CHAR_QUOTE, STATE_END_QUOTE)                          \
  X(STATE_END_QUOTE, CHAR_QUOTE, STATE_END_DQUOTE)                             \
  X(STATE_END_DQUOTE, CHAR_QUOTE, STATE_END_MULTILINE_STR)

// arrray where indexing by tuple (STATE_CUR, CHAR_TYPE) yields STATE_NEXT
// initially set to STATE_END
static State_t m_transitionTable[STATE_CNT][CHAR_TYPE_CNT] = {STATE_END};

// overwrite values at specific indices
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

      // STATE_STRING
      // all CHARS constitute the string except CHAR_QUOTE
      // CHAR_QUOTE handled in transitions macro
      else if (idx == STATE_STRING) {
        m_transitionTable[idx][idy] = STATE_STRING;
      }

      // same for multiline string
      // all CHARS default to MULTILINE_STR except on '"'
      // CHAR_QUOTE handled in transitions macro
      else if (idx == STATE_MULTILINE_STR || idx == STATE_END_QUOTE ||
               idx == STATE_END_DQUOTE) {
        m_transitionTable[idx][idy] = STATE_MULTILINE_STR;
      } else {
        m_transitionTable[idx][idy] = STATE_END;
      }
    }
  }

#define X(curr, ch, next) m_transitionTable[curr][ch] = next;
  X_TRANSITIONS(X)
#undef X
}

/* maps the tuple (curr, type) to the next state
 * curr - current state
 * type - type of the current char
 * return - next state
 */
static inline State_t NextState(State_t curr, CharType_t type) {
  return m_transitionTable[curr][type];
}

#endif // LEXER_FSM_H
