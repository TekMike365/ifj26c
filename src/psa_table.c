/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "psa_table.h"

/* Table of terminals */
const char PTable[18][18] = {
    {' ', ' ', '>', ' ', ' ', '=', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {' ', ' ', '>', ' ', ' ', ' ', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '<', '<', '<', '<', '=', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', '<', ' '},
    {' ', ' ', '>', ' ', ' ', ' ', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>',
     '>', '>', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ',
     ' ', ' ', '>'},
    {'<', '<', '<', '<', '<', '<', ' ', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', '<', ' '},
};

/* returns symbol (terminal) from token type */
PTSymbol_t SymFromToken(TokenType_t t) {
  switch (t) {
  case TOKEN_IDENTIFIER:
    return PT_ID;
  case TOKEN_INT:
    return PT_VAL;
  case TOKEN_ZERO:
    return PT_VAL;
  case TOKEN_DOUBLE:
    return PT_VAL;
  case TOKEN_STRING:
    return PT_VAL;
  case TOKEN_COMMA:
    return PT_COMMA;
  case TOKEN_LPAR:
    return PT_LPAR;
  case TOKEN_RPAR:
    return PT_RPAR;
  case TOKEN_STAR:
    return PT_MUL;
  case TOKEN_SLASH:
    return PT_DIV;
  case TOKEN_PLUS:
    return PT_PLUS;
  case TOKEN_UNARY_PLUS:
    return PT_UPLUS;
  case TOKEN_MINUS:
    return PT_MINUS;
  case TOKEN_UNARY_MINUS:
    return PT_UMINUS;
  case TOKEN_LESS:
    return PT_LT;
  case TOKEN_LESS_EQ:
    return PT_LTE;
  case TOKEN_GREATER:
    return PT_GT;
  case TOKEN_GREATER_EQ:
    return PT_GTE;
  case TOKEN_DOUBLE_EQ:
    return PT_EQ;
  case TOKEN_NOT_EQ:
    return PT_NEQ;
  default:
    return PT_DOLLAR;
  }
}

/* returns operator type from symbol (terminal) */
Operator_t OpFromSym(PTSymbol_t sym) {
  switch (sym) {
  case PT_EQ:
    return OP_EQ;
  case PT_NEQ:
    return OP_NEQ;
  case PT_LT:
    return OP_LT;
  case PT_LTE:
    return OP_LTE;
  case PT_GT:
    return OP_GT;
  case PT_GTE:
    return OP_GTE;
  case PT_PLUS:
    return OP_PLUS;
  case PT_MINUS:
    return OP_MINUS;
  case PT_DIV:
    return OP_DIV;
  case PT_MUL:
    return OP_MUL;
  default:
    return OP_ERR;
  }
}