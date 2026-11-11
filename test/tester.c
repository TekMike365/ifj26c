/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "tester.h"
#include <stdio.h>

// these are used to increment test status
int g_testsPassed = 0;
int g_testsFailed = 0;
int g_testsTotal = 0;

// signals whether current test failed
// ASSERT macros change its value to 1 on FAIL
int g_currentFailed = 0;

// call a unit test function TestFunc
// update test status variables
void RunTest(void (*TestFunc)(void), const char *testName) {
  g_currentFailed = 0; // reset for a new test
  g_testsTotal++;
  printf("%s: ", testName);

  TestFunc(); // call unit test function

  if (g_currentFailed == 1) {
    g_testsFailed++;
  } else {
    g_testsPassed++;
    printf("[PASS]\n");
  }
}

void SuiteInit(const char *testName) {
  printf("\n========== RUNNING TEST SUITE: %s ==========\n", testName);
}

void PrintSummary(void) {
  puts("\n========== SUITE FINISHED ==========\n");

  printf("PASSED: %d\n", g_testsPassed);
  printf("FAILED: %d\n", g_testsFailed);
  printf("TOTAL:  %d\n", g_testsTotal);
}
