/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "tester.h"
#include <stdio.h>
#include <string.h>

#define TESTFUNC(name, test_input, test_body)                                  \
  void name(void) {                                                            \
    FILE *tmp = tmpfile();                                                     \
    ASSERT_TRUE(tmp != NULL);                                                  \
                                                                               \
    fputs((test_input), tmp);                                                  \
                                                                               \
    rewind(tmp);                                                               \
                                                                               \
    LayoutFilter_t filter;                                                     \
    ASSERT_TRUE(LayoutFilterInit(&filter, tmp));                               \
                                                                               \
    {test_body} LayoutFilterTerminate(&filter);                                \
    fclose(tmp);                                                               \
  }

TESTFUNC(RecognizeIdentifier, "myVar", {
  Token_t next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_IDENTIFIER);
})

TESTFUNC(RecognizeKeyword, "for", {
  Token_t next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_KEYWORD_FOR);
})

TESTFUNC(RecognizeInt, "12343", {
  Token_t next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
})

TESTFUNC(RecognizeIntUnderscore, "12_34_3", {
  Token_t next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
})

TESTFUNC(RecognizeEOF, "Int", {
  Token_t next = GetNextToken(&filter);
  next = GetNextToken(&filter);
  next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_EOF);
})

TESTFUNC(RecognizeNewline, "i\n", {
  Token_t next = GetNextToken(&filter);
  next = GetNextToken(&filter);
  ASSERT_INT_EQ(next.type, TOKEN_NEWLINE);
})

  Token_t next = GetNextToken(&filter);
  next = GetNextToken(&filter);

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
