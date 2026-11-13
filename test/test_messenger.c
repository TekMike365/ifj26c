/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 *
 * A very simple message test.
 *
 * TODO: make a bash script test that would generate and run multiple programs
 */
#include "src/messenger.h"
#include <stdio.h>

int main(void) {
  MessengerInit();
  MessengerStoreNewLine(ftell(stdin));

  for (int c, i = 0; (c = getc(stdin)) != EOF && i < 6; i++)
    ;

  printf("<TEST> at: %ld\n", ftell(stdin));

  Message_t *msg = MsgStart(MSG_INFO, 67, "Dis a message.");
  MsgAddContext(msg, MSG_CONTEXT_T(0), "In function `%s'", "foo()");
  MsgAddContext(
      msg,
      MSG_CONTEXT_T(.lineNum = 1, .hlStartCol = 4, .hlEndCol = 15, .poiCol = 7),
      "wrong, you're wrong here");
  MsgSend(msg);

  printf("<TEST> at: %ld\n", ftell(stdin));

  MessengerTerminate();
  return 0;
}
