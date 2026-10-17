/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "tester.h"

void RecognizeIdentifier(void) {
  Lexer_t lexer;
  LexerInit(&lexer, "myVar");

  Token_t next = NextToken(&lexer);

  ASSERT_INT_EQ(next.type, TOKEN_IDENTIFIER);
}

void RecognizeInt(void) {
  Lexer_t lexer;
  LexerInit(&lexer, "12343");

  Token_t next = NextToken(&lexer);

  ASSERT_INT_EQ(next.type, TOKEN_INT);
}

int main(void) {
  SuiteInit("lexer");

  RunTest(RecognizeIdentifier, "Recognize identifier");
  RunTest(RecognizeInt, "Recognize int");

  PrintSummary();
  return 0;
}
