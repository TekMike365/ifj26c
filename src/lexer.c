/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "char_buf.h"
#include "lexer_fsm.h"
#include <assert.h>
#include <string.h>

/**TODO:
 * FSM states requiring separate handling:
 * COMMENT - single line, block
 *
 * START:WHITESPACE -> START?
 *
 * parseIdentifier(char *str)
 *
 * make TRANSITION TABLE 1D
 */

// ##############################################################
//                    LEXER.C INTERNAL STUFF
// ##############################################################

static inline TokenType_t matchKeywordSuffix(const char *str, const char *ref,
                                             size_t len) {

  if (memcmp(str, ref, len) == 0) {
    return TOKEN_KEYWORD;
  }

  return TOKEN_IDENTIFIER;
}

/* decides whether str is identifier or keyword
 * implemented using length-partition followed by trie matching
 * str - pointer to a char array
 * return - TOKEN_NONE on str == NULL, TOKEN_KEYWORD on keyword, else
 * TOKEN_IDENTIFIER
 */
static TokenType_t ParseIdentifier(const char *str) {
  if (!str)
    return TOKEN_NONE;

  size_t len = strlen(str);

  // shortest keyword (do/if) has len 2
  // longest (extension) has 9
  if (len < 2 || len > 9) {
    return TOKEN_IDENTIFIER;
  }
  // partition keyword set by length
  switch (len) {
  case 2:
    // trie search
    switch (str[0]) {
    case 'd':
      // match the rest with "o" as in "do"
      //                    rest has len 1 ---v
      return matchKeywordSuffix(str + 1, "o", 1); // 'do'
    case 'i':
      return matchKeywordSuffix(str + 1, "f", 1); // 'if'
    }
    break;
  case 3:
    switch (str[0]) {
    case 'd':
      return matchKeywordSuffix(str + 1, "ef", 2); // 'def'
    case 'f':
      return matchKeywordSuffix(str + 1, "or", 2); // 'for'
    case 'I':
      return matchKeywordSuffix(str + 1, "nt", 2); // 'Int'
    case 'v':
      switch (str[1]) {
        // annoying, both ('var'/'val') start as 'va' and have length 3 :(
        // kept switch for consistency
      case 'a':
        switch (str[2]) {
        case 'l':
          return TOKEN_KEYWORD; // 'val'

        case 'r':
          return TOKEN_KEYWORD; // 'var'
        }
      }
    }
    break;

  case 4:
    switch (str[0]) {
    case 'c':
      return matchKeywordSuffix(str + 1, "ase", 3); // 'case'
    case 'e':
      return matchKeywordSuffix(str + 1, "lse", 3); // 'else'
    case 'L':
      return matchKeywordSuffix(str + 1, "ist", 3); // 'List'
    case 't':
      return matchKeywordSuffix(str + 1, "ype", 3); // 'type'
    case 'U':
      return matchKeywordSuffix(str + 1, "nit", 3); // 'Unit'
    }
    break;

  case 5:
    switch (str[0]) {
    case 'c':
      return matchKeywordSuffix(str + 1, "lass", 4); // 'class'
    case 'm':
      return matchKeywordSuffix(str + 1, "atch", 5); // 'match'
    case 'w':
      return matchKeywordSuffix(str + 1, "hile", 4); // 'while'
    }
    break;
  case 6:
    switch (str[0]) {
    case 'i':
      return matchKeywordSuffix(str + 1, "mport", 5); // 'import'
    case 'r':
      return matchKeywordSuffix(str + 1, "eturn", 5); // 'return'
    case 'S':
      return matchKeywordSuffix(str + 1, "tring", 5); // 'String'
    }
    break;
  case 7:
    return matchKeywordSuffix(str + 0, "Boolean", 7); // 'Boolean'
    // no keyword with length 8
  case 8:
    return TOKEN_IDENTIFIER;
  case 9:
    return matchKeywordSuffix(str + 0, "extension", 9); // 'extension'

  default:
    return TOKEN_IDENTIFIER;
  }
  return TOKEN_IDENTIFIER;
}

char *InStringpool(char *str) {
  (void)str;
  return NULL;
}

// ##############################################################
//                    LEXER.H IMPLEMENTATION
// ##############################################################

/** initialize a lexer struct
 * lexer - pointer to a lexer struct
 * src - pointer to buffer to scan
 */
void LexerInit(Lexer_t *lexer, FILE *src) {
  assert(lexer != NULL);

  lexer->src = src;
  CharBufInit(&lexer->buf);
  lexer->offset = 0;
  lexer->line = 1; // first line

  // initialize the map and transition table
  ASCIIMapInit();
  TransitionTableInit();
}

void LexerTerminate(Lexer_t *lexer) {
  if (lexer == NULL) {
    return;
  }

  lexer->src = NULL;
  CharBufTerminate(&lexer->buf);
  lexer->line = 1;
  lexer->offset = 0;
}

Token_t CreateToken(void) {
  Token_t new = {.lexemeS = NULL, .line = 0, .len = 0, .type = TOKEN_NONE};
  return new;
}

/* TODO
 * count newlines
 * string buff
 * string pool
 */
Token_t NextToken(Lexer_t *lexer) {

  // create new token
  Token_t new = CreateToken();

  // reset buffer
  CharBufClear(&lexer->buf);

  State_t current = STATE_START;
  State_t next;

  // count of processed chars
  size_t read_cnt = 0;

  int c;
  while ((c = fgetc(lexer->src)) != EOF) {

    CharType_t type = MapChar(c);

    CharBufInsert(&lexer->buf, c);

    next = NextState(current, type);
    if (next == STATE_END) {
      break;
    }

    current = next;
    read_cnt++;
  }

  // null-terminate
  CharBufInsert(&lexer->buf, '\0');

  // can be optimized by STATE->TOKEN_TYPE map
  switch (current) {

  case STATE_IDENTIFIER:
    ParseIdentifier(lexer->buf.buf);
    new.type = TOKEN_IDENTIFIER;
    break;

  case STATE_EQ:
  case STATE_NOT_EQ:
  case STATE_DOUBLE_EQ:
  case STATE_GREATER:
  case STATE_GREATER_EQ:
  case STATE_LESS:
  case STATE_LESS_EQ:
  case STATE_STAR:
  case STATE_PLUS:
  case STATE_MINUS:
  case STATE_SLASH:
    new.type = TOKEN_OPERATOR;
    break;

  case STATE_LPAR:
    new.type = TOKEN_LPAR;
    break;

  case STATE_RPAR:
    new.type = TOKEN_RPAR;
    break;

  case STATE_UNIT:
    new.type = TOKEN_UNIT;
    break;

  case STATE_INT:
    new.type = TOKEN_INT;
    // something ala strtol() to convert
    break;

  case STATE_DOUBLE:
  case STATE_EXPONENT:
  case STATE_END_DOUBLE:
    new.type = TOKEN_DOUBLE;
    // strtod()
    break;

  case STATE_DOT:
    new.type = TOKEN_DOT;
    break;

  case STATE_SEMICOLON:
    new.type = TOKEN_SEMICOLON;
    break;

  case STATE_COMMA:
    new.type = TOKEN_COMMA;
    break;

  case STATE_COLON:
    new.type = TOKEN_COLON;
    break;

  case STATE_END_STRING:
  case STATE_END_MULTILINE_STR:
    new.type = TOKEN_STRING;
    break;

  default:
    new.type = TOKEN_ERROR;
    break;
  }

  // update lexer`s position
  // TODO: something ala ungetc
  // now consumes lexeme + 1 char
  lexer->offset += read_cnt;

  return new;
}
