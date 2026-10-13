/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef TESTER_H
#define TESTER_H

#include <stdio.h>
#include <string.h>

// these are used to increment test status
extern int g_testsPassed;
extern int g_testsFailed;
extern int g_testsTotal;

// signals whether current test failed
// ASSERT macros change its value to 1 on FAIL
extern int g_currentFailed;

// call a unit test function TestFunc
void RunTest(void (*TestFunc)(void), const char *testName);

// print RUNNING SUITE: testName
void SuiteInit(const char *testName);

void PrintSummary(void);

// assert that the expression expr is true
#define ASSERT_TRUE(expr)                                                      \
  do {                                                                         \
    if (!(expr)) {                                                             \
      printf("[FAIL] %s, %d: Expected TRUE for '%s'\n", __FILE__, __LINE__,    \
             #expr);                                                           \
      g_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that the integer val equals reference ref
#define ASSERT_INT_EQ(val, ref)                                                \
  do {                                                                         \
    if ((val) != (ref)) {                                                      \
      printf("[FAIL] %s, %d: Expected '%s' (%d) equal '%s' (%d)\n", __FILE__,  \
             __LINE__, #val, (val), #ref, (ref));                              \
      g_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that string str equals reference ref
#define ASSERT_STR_EQ(str, ref)                                                \
  do {                                                                         \
    if (strcmp((str), (ref)) != 0) {                                           \
      printf("[FAIL] %s, %d: Expected '%s' (%s) match '%s' (%s)\n", __FILE__,  \
             __LINE__, #str, (str), #ref, (ref));                              \
      g_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

// assert that pointer ptr is not null
#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      printf("[FAIL] %s, %d: Pointer '%s' is NULL\n", __FILE__, __LINE__,      \
             #ptr);                                                            \
      g_currentFailed = 1;                                                     \
      return;                                                                  \
    }                                                                          \
  } while (0)

#endif
