/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "tester.h"
#include <stdio.h>
#include <string.h>

void RecognizeIdentifier(void) {

  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("myVar", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = NextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_IDENTIFIER);
  LexerTerminate(&lexer);
}

void RecognizeInt(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("12343", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = NextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  LexerTerminate(&lexer);
}

int main(void) {
  SuiteInit("lexer");

  RunTest(RecognizeIdentifier, "Recognize identifier");
  RunTest(RecognizeInt, "Recognize int");

  PrintSummary();
  return 0;
}
