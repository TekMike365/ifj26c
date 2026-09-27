/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "test_framework.h"
#include "tests.h" // contains tests declarations

// THESE TWO ARE JUST FOR ILLUSTRATION
// THEIR DECLARATIONS SHOULD BE IN tests.h
// THEIR DEFINITIONS SHOULD BE IN A SEPARATE
// SOURCE FILE (i.e. dummy_pass.c)
void dummy_pass() { ASSERT_TRUE(1); }
void dummy_fail() { ASSERT_TRUE(0); }

int main(void) {
  printf("\n==========RUNNING TESTS==========\n");

  run_test(dummy_pass, "dummy pass");
  run_test(dummy_fail, "dummy fail");

  print_summary();
  return 0;
}
