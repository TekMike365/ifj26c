/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "tester.h"

// THESE ARE JUST FOR ILLUSTRATION
void DummyPass(void) { ASSERT_TRUE(1); }
void DummyFail(void) { ASSERT_TRUE(0); }
void TestIntEq(void) { ASSERT_INT_EQ(2, 3); }
void TestNull(void) { ASSERT_NOT_NULL(NULL); }
void TestStrEq(void) { ASSERT_STR_EQ("ab", "ab"); }

int main(void) {
  SuiteInit("dummy");

  RunTest(DummyPass, "dummy pass");
  RunTest(DummyFail, "dummy fail");
  RunTest(TestIntEq, "int eq");
  RunTest(TestNull, "test null");
  RunTest(TestStrEq, "str eq");

  PrintSummary();
  return 0;
}
