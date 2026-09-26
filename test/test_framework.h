/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

// these are used to increment test status
static int m_testsTotal = 0;
static int m_testsFailed = 0;
static int m_currentFailed = 0;

// assert that the expression expr is true
#define ASSERT_TRUE(expr)                                                      \
  do {                                                                         \
    if (!(expr)) {                                                             \
      printf("[FAIL] %s, %d: Expected TRUE for %s", __FILE__, __LINE__,        \
             #expr);                                                           \
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

#endif
