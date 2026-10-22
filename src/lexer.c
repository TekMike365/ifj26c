/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "char_buf.h"
#include "lexer_fsm.h"
#include <assert.h>

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

TokenType_t ParseIdentifier(char *str) {
  (void)str;
  // hash
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

  State_t current = STATE_START;
  State_t next;

  // count of processed chars
  size_t read_cnt = 0;

  int c;
  while ((c = fgetc(lexer->src)) != EOF) {

    CharType_t type = MapChar(c);

    next = NextState(current, type);
    if (next == STATE_END) {
      break;
    }

    current = next;
    read_cnt++;
  }

  // can be optimized by STATE->TOKEN_TYPE map
  switch (current) {

  case STATE_IDENTIFIER:
    // IsKeyword()
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
