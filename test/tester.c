/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "tester.h"

// these are used to increment test status
int m_testsPassed = 0;
int m_testsFailed = 0;
int m_testsTotal = 0;

// signals whether current test failed
// ASSERT macros change its value to 1 on FAIL
int m_currentFailed = 0;

// call a unit test function TestFunc
// update test status variables
void RunTest(void (*TestFunc)(void), char *testName) {
  m_currentFailed = 0; // reset for a new test
  m_testsTotal++;

  TestFunc(); // call unit test function

  if (m_currentFailed == 1) {
    m_testsFailed++;
  } else {
    m_testsPassed++;
    printf("[PASS] %s\n", testName);
  }
}

void PrintSummary(void) {
  printf("\n==========TESTS FINISHED==========\n");

  printf("PASSED: %d\n", m_testsPassed);
  printf("FAILED: %d\n", m_testsFailed);
  printf("TOTAL:  %d\n", m_testsTotal);
}
