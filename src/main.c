/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "parser.h"
#include <stdio.h>

int main(void) {
  MessengerInit();

  ErrorCode_t err = ERROR_NOERR;

  ASTNode_t *ASTree = Parse(&err);

  if (ASTree == NULL) {
    MessengerTerminate();
    return err;
  }

  ASTDump(ASTree);

  ASTFree(ASTree);

  MessengerTerminate();
  return err;
}