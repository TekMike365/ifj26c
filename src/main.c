/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "parser_internal.h"
#include "semantics.h"
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

  AError_t aerr = AnalyzeAST(ASTree);
  if (aerr == AERR_MEM) {
    Message_t *msg = MsgStart(MSG_ERROR, ERROR_INTERNAL,
                              "Critical error occured, shutting down...");
    MsgSend(msg);
    ASTFree(ASTree);
    return ERROR_INTERNAL;
  }

  ASTFree(ASTree);

  MessengerTerminate();
  return err;
}
