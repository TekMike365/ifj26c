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

  Token_t next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_IDENTIFIER);
  LexerTerminate(&lexer);
}
void RecognizeKeyword(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("for", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_KEYWORD_FOR);
  LexerTerminate(&lexer);
}

void RecognizeInt(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("12343", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
  LexerTerminate(&lexer);
}

void RecognizeIntUnderscore(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("12_34_3", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_INT);
  ASSERT_INT_EQ(next.lexemeI, 12343);
  LexerTerminate(&lexer);
}

void RecognizeEOF(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("Int", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = GetNextToken(&lexer);
  next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_EOF);
  LexerTerminate(&lexer);
}

void RecognizeNewline(void) {
  FILE *tmp = tmpfile();
  ASSERT_TRUE(tmp != NULL);

  fputs("\n", tmp);

  rewind(tmp);

  Lexer_t lexer;
  LexerInit(&lexer, tmp);

  Token_t next = GetNextToken(&lexer);

  fclose(tmp);
  ASSERT_INT_EQ(next.type, TOKEN_NEWLINE);
  LexerTerminate(&lexer);
}

int main(void) {
  SuiteInit("lexer");

  RunTest(RecognizeIdentifier, "Recognize identifier");
  RunTest(RecognizeKeyword, "Recongize keyword");
  RunTest(RecognizeInt, "Recognize int");
  RunTest(RecognizeIntUnderscore, "Recognize int with underscore");
  RunTest(RecognizeEOF, "Recognize EOF");
  RunTest(RecognizeNewline, "Recognize newline");

  PrintSummary();
  return 0;
}
