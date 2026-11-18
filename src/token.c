/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "token.h"
#include "error.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* returns a nulled TOKEN_NONE token */
Token_t CreateToken(void) {
  Token_t new = {.lexemeS = NULL, .pos = {0}, .type = TOKEN_NONE};
  return new;
}

// terminates a token
// if TOKEN_IDENTIFIER or TOKEN_STRING
// if lexemeS is not NULL, free it
void TerminateToken(Token_t *token) {
  if (!token)
    return;

  if (token->type == TOKEN_IDENTIFIER || token->type == TOKEN_STRING) {
    if (token->lexemeS) {
      free(token->lexemeS);
      token->lexemeS = NULL;
    }
  }
  *token = (Token_t){.pos = {0}, .type = TOKEN_NONE};
}

// returns a token of the specified type
Token_t MakeToken(TokenType_t type) {
  Token_t new = CreateToken();
  new.type = type;
  return new;
}

/* Sets a token to TOKEN_ERROR with the corresponding error code
 * * error - error code (MEM/LEX)
 * * token - token to be set
 */
void TokenSetError(ErrorCode_t error, Token_t *token) {
  assert(token != NULL);
  token->type = TOKEN_ERROR;
  token->error = error;
}

/* Allocates memory for lexemeS and copies src into it
 * * token - token whose lexemeS is to be set
 * * src - source string
 * * length - length of the string
 * returns 0 on malloc failure, else 1
 */
_Bool TokenSetLexemeS(Token_t *token, const char *src, size_t length) {
  assert(token != NULL);
  assert(src != NULL);

  token->lexemeS = malloc(length * sizeof(char));
  if (!token->lexemeS) {
    return 0;
  }
  memcpy(token->lexemeS, src, length);
  return 1;
}
