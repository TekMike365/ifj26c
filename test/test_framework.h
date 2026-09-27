/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

// these are used to increment test status
static int m_testsPassed = 0;
static int m_testsFailed = 0;
static int m_testsTotal = 0;

// signals whether current test failed
// ASSERT macros change its value to 1 on FAIL
static int m_currentFailed = 0;

// assert that the expression expr is true
#define ASSERT_TRUE(expr)                                                      \
  do {                                                                         \
    if (!(expr)) {                                                             \
      printf("[FAIL] %s, %d: Expected TRUE for %s\n", __FILE__, __LINE__,      \
             #expr);                                                           \
      m_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that the integer val equals reference ref
#define ASSERT_INT_EQ(val, ref)                                                \
  do {                                                                         \
    if ((val) != (ref)) {                                                      \
      printf("[FAIL] %s, %d: Expected %s (%d) equal %s (%d)\n", __FILE__,      \
             __LINE__, #val, (val), #ref, (ref));                              \
      m_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that string str equals reference ref
#define ASSERT_STR_EQ(str, ref)                                                \
  do {                                                                         \
    if (strcmp((str), (ref)) != 0) {                                           \
      printf("[FAIL] %s, %d: Expected %s (%s) match %s (%s)\n", __FILE__,      \
             __LINE__, #str, (str), #ref, (ref));                              \
      m_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that pointer ptr is not null
#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      printf("[FAIL] %s, %d: Pointer %s is NULL\n", __FILE__, __LINE__, #ptr); \
      m_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

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

#endif
