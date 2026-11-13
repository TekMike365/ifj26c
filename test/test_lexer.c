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

  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_IDENTIFIER);
  LayoutFilterTerminate(&filter);
}
void RecognizeKeyword(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("for", tmp);

  rewind(tmp);

  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_KEYWORD_FOR);
  LayoutFilterTerminate(&filter);
}

void RecognizeInt(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("12343", tmp);

  rewind(tmp);

  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
  LayoutFilterTerminate(&filter);
}

void RecognizeIntUnderscore(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("12_34_3", tmp);

  rewind(tmp);
  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
  LayoutFilterTerminate(&filter);
}

void RecognizeEOF(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("Int", tmp);

  rewind(tmp);
  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_EOF);
  LayoutFilterTerminate(&filter);
}

void RecognizeNewline(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("\n", tmp);

  rewind(tmp);
  LayoutFilter_t filter;
  ASSERT_TRUE(LayoutFilterInit(&filter, tmp));

  Token_t next = GetNextToken(&filter);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_NEWLINE);
  LayoutFilterTerminate(&filter);
}

int main(void) {
  SuiteInit("filter");

  RunTest(RecognizeIdentifier, "Recognize identifier");
  RunTest(RecognizeKeyword, "Recongize keyword");
  RunTest(RecognizeInt, "Recognize int");
  RunTest(RecognizeIntUnderscore, "Recognize int with underscore");
  RunTest(RecognizeEOF, "Recognize EOF");
  RunTest(RecognizeNewline, "Recognize newline");

  PrintSummary();
  return 0;
}
