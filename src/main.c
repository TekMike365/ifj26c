/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "parser.h"
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

  AError_t aerr = AnalyzeAST(ASTree);
  if (aerr == AERR_MEM) {
    Message_t *msg = MsgStart(MSG_ERROR, ERROR_INTERNAL,
                              "Critical error occured, shutting down...");
    MsgSend(msg);
    ASTFree(ASTree);
    return ERROR_INTERNAL;
  }

  ASTDump(ASTree);

  if (aerr != AERR_OK || err != ERROR_NOERR) {
    Message_t *msg = MsgStart(MSG_INFO, ERROR_SEM_OTHER,
                              "Errors were detected. Skiping generation.");
    MsgSend(msg);
    ASTFree(ASTree);
    return 1;
  }

  ASTFree(ASTree);

  MessengerTerminate();
  return err;
}
