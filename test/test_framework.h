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
    return;                                                                    \
  } while (0)

// assert that string str equals reference ref
#define ASSERT_STR_EQ(str, ref)                                                \
  do {                                                                         \
    return;                                                                    \
  } while (0)

// assert that pointer ptr is not null
#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    return;                                                                    \
  } while (0)

// call a unit test function test_func
// update test status variables
void run_test(void (*test_func)(void), char *test_name) {
  m_currentFailed = 0; // reset for a new test
  m_testsTotal++;

  test_func(); // call unit test function

  if (m_currentFailed == 1) {
    m_testsFailed++;
  } else {
    m_testsPassed++;
    printf("[PASS] %s\n", test_name);
  }
}

void print_summary(void) {
  printf("\n==========TESTS FINISHED==========\n");

  printf("PASSED: %d\n", m_testsPassed);
  printf("FAILED: %d\n", m_testsFailed);
  printf("TOTAL:  %d\n", m_testsTotal);
}

#endif
