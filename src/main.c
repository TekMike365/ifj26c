/* Implementace imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */

#include "ifjcode26.h"
#include "error.h"
#include "parser.h"
#include "semantics.h"

#include <stdio.h>

int main(void) {
  MessengerInit();

  ErrorCode_t parseError = ERROR_NOERR;
  ASTNode_t *program = Parse(&parseError);
  if (program == NULL) {
    if (parseError == ERROR_NOERR) {
      ReportInternalError("Parser returned no program without an error.");
      parseError = ERROR_INTERNAL;
    }
    MessengerTerminate();
    return parseError;
  }
  if (parseError != ERROR_NOERR) {
    ASTFree(program);
    MessengerTerminate();
    return parseError;
  }

  AError_t analysisError = AnalyzeAST(program);
  if (analysisError != AERR_OK) {
    if (analysisError == AERR_MEM) {
      ReportInternalError("Out of memory during semantic analysis.");
    }
    ASTFree(program);
    MessengerTerminate();
    return analysisError == AERR_MEM ? ERROR_INTERNAL : ERROR_SEM_OTHER;
  }

  if (!IFJCode26Generate(program, stdout) || fflush(stdout) != 0) {
    ReportInternalError("Unable to generate IFJcode26 output.");
    ASTFree(program);
    MessengerTerminate();
    return ERROR_INTERNAL;
  }

  ASTFree(program);
  MessengerTerminate();
  return ERROR_NOERR;
}
