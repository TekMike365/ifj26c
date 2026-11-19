/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "error.h"
#include "messenger.h"
#include <stddef.h>

void ReportLexicalError(MsgContext_t ctx, const char *code, const char *msg,
                        _Bool addContext) {
  Message_t *message = MsgStart(MSG_ERROR, ERROR_LEX, "%s", msg);

  if (addContext) {
    MsgAddContext(message, MSG_CONTEXT_T(0), "%s", code);
    MsgAddContext(message, ctx, "here");
  }

  MsgSend(message);
}

void ReportInternalError(const char *msg) {
  Message_t *message = MsgStart(MSG_ERROR, ERROR_INTERNAL, "%s", msg);
  MsgSend(message);
}
