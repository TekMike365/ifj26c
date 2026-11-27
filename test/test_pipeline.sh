#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
compiler="${IFJ26C_COMPILER:-$repo_root/ifj26c}"
interpreter="${IFJCODE_INTERPRETER:-$repo_root/ic26int-linux-x86_64}"
interpreter_timeout="${IFJCODE_TIMEOUT:-5s}"

if [[ ! -x "$compiler" ]]; then
  printf 'Compiler is missing or not executable: %s\n' "$compiler" >&2
  exit 2
fi

if [[ "$interpreter" == */* ]]; then
  if [[ ! -x "$interpreter" ]]; then
    printf 'IFJcode26 interpreter is missing or not executable: %s\n' \
      "$interpreter" >&2
    printf 'Set IFJCODE_INTERPRETER to its executable path.\n' >&2
    exit 2
  fi
elif ! command -v "$interpreter" >/dev/null 2>&1; then
  printf 'IFJcode26 interpreter was not found: %s\n' "$interpreter" >&2
  printf 'Set IFJCODE_INTERPRETER to its executable path.\n' >&2
  exit 2
fi

if ! command -v timeout >/dev/null 2>&1; then
  printf 'The timeout command is required to run end-to-end tests.\n' >&2
  exit 2
fi

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/ifj26c-e2e.XXXXXX")"
case_names=()
passed=0
failed=0
current_case=""

cleanup() {
  local exit_status=$?
  if [[ "$exit_status" -ne 0 && -n "$current_case" ]]; then
    failed=$((failed + 1))
  fi

  printf '\nTest summary: %d passed, %d failed\n' "$passed" "$failed"

  for name in "${case_names[@]}"; do
    rm -f -- "$tmp_dir/$name.src" "$tmp_dir/$name.ifj" \
      "$tmp_dir/$name.expected" "$tmp_dir/$name.actual" \
      "$tmp_dir/$name.compiler.stderr" "$tmp_dir/$name.interpreter.stderr"
  done
  rmdir -- "$tmp_dir"
  return "$exit_status"
}
trap cleanup EXIT

track_case_files() {
  local name="$1"
  case_names+=("$name")
  current_case="$name"
}

record_pass() {
  local name="$1"
  local result="${2:-}"
  passed=$((passed + 1))
  current_case=""
  printf 'PASS: %s%s\n' "$name" "$result"
}

run_case() {
  local name="$1"
  local expected="$2"
  local input="$3"
  track_case_files "$name"
  local source_file="$tmp_dir/$name.src"
  local code_file="$tmp_dir/$name.ifj"
  local expected_file="$tmp_dir/$name.expected"
  local actual_file="$tmp_dir/$name.actual"
  local compiler_stderr="$tmp_dir/$name.compiler.stderr"
  local interpreter_stderr="$tmp_dir/$name.interpreter.stderr"

  cat >"$source_file"
  printf '%s' "$expected" >"$expected_file"

  if ! "$compiler" <"$source_file" >"$code_file" 2>"$compiler_stderr"; then
    printf 'FAIL: %s (compilation failed)\n' "$name" >&2
    cat "$compiler_stderr" >&2
    exit 1
  fi

  if printf '%s' "$input" |
    timeout "$interpreter_timeout" "$interpreter" "$code_file" \
      >"$actual_file" 2>"$interpreter_stderr"; then
    :
  else
    local status=$?
    if [[ "$status" -eq 124 ]]; then
      printf 'FAIL: %s (IFJcode26 execution timed out)\n' "$name" >&2
    else
      printf 'FAIL: %s (IFJcode26 execution failed with status %s)\n' \
        "$name" "$status" >&2
    fi
    cat "$interpreter_stderr" >&2
    exit 1
  fi

  if ! cmp -s "$expected_file" "$actual_file"; then
    printf 'FAIL: %s (output mismatch)\n' "$name" >&2
    diff -u "$expected_file" "$actual_file" >&2 || true
    exit 1
  fi

  record_pass "$name"
}

run_compile_rejection() {
  local name="$1"
  local expected_status="$2"
  track_case_files "$name"
  local source_file="$tmp_dir/$name.src"
  local code_file="$tmp_dir/$name.ifj"
  local compiler_stderr="$tmp_dir/$name.compiler.stderr"
  local status

  cat >"$source_file"
  if "$compiler" <"$source_file" >"$code_file" 2>"$compiler_stderr"; then
    printf 'FAIL: %s (invalid program was accepted)\n' "$name" >&2
    exit 1
  else
    status=$?
  fi

  if [[ "$status" -ne "$expected_status" ]]; then
    printf 'FAIL: %s (expected compiler exit %s, got %s)\n' \
      "$name" "$expected_status" "$status" >&2
    cat "$compiler_stderr" >&2
    exit 1
  fi
  if [[ -s "$code_file" || ! -s "$compiler_stderr" ]]; then
    printf 'FAIL: %s (expected diagnostics and no generated code)\n' \
      "$name" >&2
    cat "$compiler_stderr" >&2
    exit 1
  fi

  record_pass "$name" " (rejected)"
}

run_runtime_rejection() {
  local name="$1"
  local input="$2"
  local expected_status="${3:-}"
  track_case_files "$name"
  local source_file="$tmp_dir/$name.src"
  local code_file="$tmp_dir/$name.ifj"
  local actual_file="$tmp_dir/$name.actual"
  local compiler_stderr="$tmp_dir/$name.compiler.stderr"
  local interpreter_stderr="$tmp_dir/$name.interpreter.stderr"
  local status

  cat >"$source_file"
  if ! "$compiler" <"$source_file" >"$code_file" 2>"$compiler_stderr"; then
    printf 'FAIL: %s (compilation failed unexpectedly)\n' "$name" >&2
    cat "$compiler_stderr" >&2
    exit 1
  fi

  if printf '%s' "$input" |
    timeout "$interpreter_timeout" "$interpreter" "$code_file" \
      >"$actual_file" 2>"$interpreter_stderr"; then
    printf 'FAIL: %s (runtime error was expected)\n' "$name" >&2
    exit 1
  else
    status=$?
  fi
  if [[ "$status" -eq 124 ]]; then
    printf 'FAIL: %s (IFJcode26 execution timed out)\n' "$name" >&2
    cat "$interpreter_stderr" >&2
    exit 1
  fi
  if [[ -n "$expected_status" && "$status" -ne "$expected_status" ]]; then
    printf 'FAIL: %s (expected runtime exit %s, got %s)\n' \
      "$name" "$expected_status" "$status" >&2
    cat "$interpreter_stderr" >&2
    exit 1
  fi
  if [[ "$status" -eq 0 ]]; then
    printf 'FAIL: %s (runtime unexpectedly succeeded)\n' "$name" >&2
    exit 1
  fi

  record_pass "$name" " (runtime error)"
}

run_timeout_rejection() {
  local name="$1"
  local duration="$2"
  local input="$3"
  track_case_files "$name"
  local source_file="$tmp_dir/$name.src"
  local code_file="$tmp_dir/$name.ifj"
  local actual_file="$tmp_dir/$name.actual"
  local compiler_stderr="$tmp_dir/$name.compiler.stderr"
  local interpreter_stderr="$tmp_dir/$name.interpreter.stderr"
  local status

  cat >"$source_file"
  if ! "$compiler" <"$source_file" >"$code_file" 2>"$compiler_stderr"; then
    printf 'FAIL: %s (compilation failed unexpectedly)\n' "$name" >&2
    cat "$compiler_stderr" >&2
    exit 1
  fi

  if printf '%s' "$input" |
    timeout "$duration" "$interpreter" "$code_file" \
      >"$actual_file" 2>"$interpreter_stderr"; then
    printf 'FAIL: %s (execution unexpectedly completed)\n' "$name" >&2
    exit 1
  else
    status=$?
  fi
  if [[ "$status" -ne 124 ]]; then
    printf 'FAIL: %s (expected timeout status 124, got %s)\n' \
      "$name" "$status" >&2
    cat "$interpreter_stderr" >&2
    exit 1
  fi

  record_pass "$name" " (timed out as expected)"
}

run_case numeric_conversion 3 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toInt(1 + 2.0))
PROGRAM

run_case boolean_input true $'true\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readBoolean())
PROGRAM

run_case function_while 123 '' <<'PROGRAM'
import ifj26.*
def increment(value: Int): Int = value + 1
@main def main() =
  var count: Int = 0
  while count < 3 do
    count = increment(count)
    print(count)
PROGRAM

run_case generator_loop 123 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 do
    print(i)
PROGRAM

run_case integer_zero 0 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(0)
PROGRAM

run_case unary_negative -5 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(-5)
PROGRAM

run_case arithmetic_precedence 7 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1 + 2 * 3)
PROGRAM

run_case floating_arithmetic 8 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toInt((1.5 + 2.5) * 2.0))
PROGRAM

run_case equality_true true '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1 == 1)
PROGRAM

run_case inequality_false false '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(3 != 3)
PROGRAM

run_case short_circuit_and false '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print((1 < 0) && readBoolean())
PROGRAM

run_case short_circuit_or true '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print((1 < 2) || readBoolean())
PROGRAM

run_case boolean_not false '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(!(1 < 2))
PROGRAM

run_case string_concat hello_world '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("hello" + "_" + "world")
PROGRAM

run_case string_escape 'a # b' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a # b")
PROGRAM

run_case escaped_newline $'a\nb' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a\nb")
PROGRAM

run_case escaped_quote 'a"b' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a\"b")
PROGRAM

run_case escaped_backslash 'a\b' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a\\b")
PROGRAM

run_case escaped_tab $'a\tb' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a\tb")
PROGRAM

run_case escaped_carriage_return $'a\rb' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("a\rb")
PROGRAM

run_case multiline_string $'first\nsecond' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("""first
second""")
PROGRAM

run_case empty_multiline_string '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("""""")
PROGRAM

run_case non_ascii_string 'héllo' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("héllo")
PROGRAM

run_case empty_string '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print("")
PROGRAM

run_case print_unit 'x()' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(print("x"))
PROGRAM

run_case zero_arg_function 42 '' <<'PROGRAM'
import ifj26.*
def answer(): Int = 42
@main def main() =
  print(answer())
PROGRAM

run_case multiple_arg_function 5 '' <<'PROGRAM'
import ifj26.*
def add(left: Int, right: Int): Int = left + right
@main def main() =
  print(add(2, 3))
PROGRAM

run_case nested_function_calls 5 '' <<'PROGRAM'
import ifj26.*
def increment(value: Int): Int = value + 1
def twice(value: Int): Int = increment(increment(value))
@main def main() =
  print(twice(3))
PROGRAM

run_case recursive_factorial 120 '' <<'PROGRAM'
import ifj26.*
def factorial(value: Int): Int =
  if value <= 1 then 1 else value * factorial(value - 1)
@main def main() =
  print(factorial(5))
PROGRAM

run_case mutual_recursion true '' <<'PROGRAM'
import ifj26.*
def isEven(value: Int): Boolean =
  if value == 0 then 1 == 1 else isOdd(value - 1)
def isOdd(value: Int): Boolean =
  if value == 0 then 1 == 0 else isEven(value - 1)
@main def main() =
  print(isEven(8))
PROGRAM

run_case int_to_double_expression 2 '' <<'PROGRAM'
import ifj26.*
def half(value: Int): Double = value / 2.0
@main def main() =
  print(toInt(half(5)))
PROGRAM

run_case if_true yes '' <<'PROGRAM'
import ifj26.*
@main def main() =
  if 1 < 2 then
    print("yes")
PROGRAM

run_case if_else no '' <<'PROGRAM'
import ifj26.*
@main def main() =
  if 2 < 1 then
    print("yes")
  else
    print("no")
PROGRAM

run_case while_zero '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  while 1 < 0 do
    print("unexpected")
PROGRAM

run_case until_generator 12 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 until 3 do
    print(i)
PROGRAM

run_case descending_generator 321 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 3 to 1 by -1 do
    print(i)
PROGRAM

run_case stepped_generator 13 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 4 by 2 do
    print(i)
PROGRAM

run_case oversized_generator_step 1 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 by 5 do
    print(i)
PROGRAM

run_case mismatched_generator_direction '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 by -1 do
    print(i)
PROGRAM

run_case descending_mismatched_generator_direction '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 3 to 1 by 1 do
    print(i)
PROGRAM

run_case nested_generators 11122122 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 2 do
    for j <- 1 to 2 do
      print(i)
      print(j)
PROGRAM

run_case generator_variable_scope 1239 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 do
    print(i)
  var i: Int = 9
  print(i)
PROGRAM

run_case local_variable_shadowing 21 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  var value: Int = 1
  if 1 < 2 then
    val value: Int = 2
    print(value)
  print(value)
PROGRAM

run_case empty_generator '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 4 to 1 do
    print(i)
PROGRAM

run_case long_integration '13|compiler' '' <<'PROGRAM'
import ifj26.*
def scale(value: Int): Double = value * 1.5
@main def main() =
  var total: Int = 0
  for i <- 1 to 4 do
    if i <= 2 then
      total = total + i
    else
      total = total + toInt(scale(i))
  print(total)
  print("|")
  print(subStr("compiler", 0, strLength("compiler")))
PROGRAM

run_case long_nested_integration '27|ab' '' <<'PROGRAM'
import ifj26.*
def weight(row: Int, column: Int): Int =
  if row == column then row * 2 else row + column
@main def main() =
  var total: Int = 0
  for row <- 1 to 3 do
    for column <- 1 to 3 do
      if row <= column then
        total = total + weight(row, column)
      else
        total = total + 1
  print(total)
  print("|")
  var count: Int = 0
  while count < 2 do
    if count == 0 then
      print("a")
    else
      print("b")
    count = count + 1
PROGRAM

run_case read_int 42 $'42\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readInt())
PROGRAM

run_case read_int_invalid nil $'not-a-number\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readInt())
PROGRAM

run_case read_int_eof nil '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readInt())
PROGRAM

run_case read_double 4 $'4.75\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toInt(readDouble()))
PROGRAM

run_case read_double_invalid nil $'not-a-number\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readDouble())
PROGRAM

run_case read_line $'hello\n' $'hello\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readLine())
PROGRAM

run_case read_line_eof '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readLine())
PROGRAM

run_case boolean_false false $'false\n' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(readBoolean())
PROGRAM

run_case to_string_int 123 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toStr(123))
PROGRAM

run_case to_string_boolean true '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toStr(1 == 1))
PROGRAM

run_case to_int_negative_fraction -2 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(toInt(0.0 - 2.9))
PROGRAM

run_case string_length 5 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(strLength("hello"))
PROGRAM

run_case character_from_code A '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(chr(65))
PROGRAM

run_case ordinal_valid 65 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(ord("A", 0))
PROGRAM

run_case ordinal_out_of_range -1 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(ord("A", 1))
PROGRAM

run_case ordinal_negative_index -1 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(ord("A", -1))
PROGRAM

run_case substring_middle ell '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(subStr("hello", 1, 4))
PROGRAM

run_case substring_empty '' '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(subStr("hello", 2, 2))
PROGRAM

run_case builtin_chain 3 '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(strLength(subStr("compiler", 2, 5)))
PROGRAM

run_runtime_rejection divide_by_zero '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1 / 0)
PROGRAM

run_runtime_rejection substring_out_of_bounds '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(subStr("abc", 0, 4))
PROGRAM

run_runtime_rejection substring_negative_start '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(subStr("abc", -1, 2))
PROGRAM

run_runtime_rejection substring_reversed_range '' <<'PROGRAM'
import ifj26.*
@main def main() =
  print(subStr("abc", 2, 1))
PROGRAM

run_runtime_rejection dynamic_zero_generator_step '' 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  val step: Int = 1 - 1
  for i <- 1 to 3 by step do
    print(i)
PROGRAM

run_timeout_rejection infinite_while 1s '' <<'PROGRAM'
import ifj26.*
@main def main() =
  var value: Int = 0
  while 1 == 1 do
    value = value + 1
PROGRAM

run_compile_rejection missing_main 10 <<'PROGRAM'
import ifj26.*
def helper(): Int = 1
PROGRAM

run_compile_rejection undefined_variable 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(missing)
PROGRAM

run_compile_rejection immutable_assignment 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  val value: Int = 1
  value = 2
PROGRAM

run_compile_rejection bad_builtin_argument 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(strLength(1))
PROGRAM

run_compile_rejection invalid_string_escape 1 <<'PROGRAM'
import ifj26.*
@main def main() =
  print("bad\q")
PROGRAM

run_compile_rejection wrong_user_argument_count 10 <<'PROGRAM'
import ifj26.*
def identity(value: Int): Int = value
@main def main() =
  print(identity())
PROGRAM

run_compile_rejection wrong_user_argument_type 10 <<'PROGRAM'
import ifj26.*
def identity(value: Int): Int = value
@main def main() =
  print(identity("not an integer"))
PROGRAM

run_compile_rejection mixed_numeric_string_addition 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1 + "not a number")
PROGRAM

run_compile_rejection mixed_numeric_string_comparison 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1 < "not a number")
PROGRAM

run_compile_rejection string_generator_start 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- "one" to 3 do
    print(i)
PROGRAM

run_compile_rejection double_generator_end 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3.0 do
    print(i)
PROGRAM

run_compile_rejection double_generator_step 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 by 1.0 do
    print(i)
PROGRAM

run_compile_rejection function_return_type_mismatch 10 <<'PROGRAM'
import ifj26.*
def identity(value: Int): Int = "not an integer"
@main def main() =
  print(identity(1))
PROGRAM

run_compile_rejection if_expression_branch_type_mismatch 10 <<'PROGRAM'
import ifj26.*
def choose(flag: Boolean): Int =
  if flag then 1 else "not an integer"
@main def main() =
  print(choose(1 == 1))
PROGRAM

run_compile_rejection zero_generator_step 10 <<'PROGRAM'
import ifj26.*
@main def main() =
  for i <- 1 to 3 by 0 do
    print(i)
PROGRAM

run_compile_rejection duplicate_function 10 <<'PROGRAM'
import ifj26.*
def duplicate(): Int = 1
def duplicate(): Int = 2
@main def main() =
  print(duplicate())
PROGRAM

run_compile_rejection unterminated_string 1 <<'PROGRAM'
import ifj26.*
@main def main() =
  print("unterminated
PROGRAM

run_compile_rejection malformed_exponent 1 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1e)
PROGRAM

run_compile_rejection trailing_numeric_separator 1 <<'PROGRAM'
import ifj26.*
@main def main() =
  print(1_)
PROGRAM

run_compile_rejection unterminated_multiline_string 1 <<'PROGRAM'
import ifj26.*
@main def main() =
  print("""unterminated
PROGRAM

run_compile_rejection inconsistent_indentation 2 <<'PROGRAM'
import ifj26.*
@main def main() =
  if 1 < 2 then
    print("first")
     print("misindented")
PROGRAM
