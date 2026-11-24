/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#ifndef IFJ26C_PSA_TABLE_H
#define IFJ26C_PSA_TABLE_H

#include "token.h"
#include "as_tree.h"

typedef enum {
  // Terminals:

  PT_ID = 0, // id
  PT_VAL,    // int, double, string literals
  PT_COMMA,  // ,
  PT_UPLUS,  // unary +
  PT_UMINUS, // unary -
  PT_LPAR,   // (
  PT_RPAR,   // )
  PT_MUL,    // *
  PT_DIV,    // /
  PT_PLUS,   // binary +
  PT_MINUS,  // binary -
  PT_LT,     // <
  PT_LTE,    // <=
  PT_GT,     // >
  PT_GTE,    // >=
  PT_EQ,     // ==
  PT_NEQ,    // !=
  PT_NEG,    // !
  PT_AND,    // &&
  PT_OR,     // ||
  PT_DOLLAR, // $

  // Nonterminals:

  PT_HANDLE,
  PT_COND,
  PT_E,
  PT_ARGS,
} PTSymbol_t;

extern const char PTable[21][21];

PTSymbol_t SymFromToken(TokenType_t t);

Operator_t OpFromSym(PTSymbol_t sym);

#endif // IFJ26C_PSA_TABLE_H