#include "parser_internal.h"
#include <stdlib.h>

// begin PSA:

#define MAX_RULE_LENGTH 4

typedef enum {
  PSA_ID = 0, // id (int, double, string)
  PSA_COMMA,  // ,
  PSA_UPLUS,  // unary +
  PSA_UMINUS, // unary -
  PSA_LPAR,   // (
  PSA_RPAR,   // )
  PSA_MUL,    // *
  PSA_DIV,    // /
  PSA_PLUS,   // binary +
  PSA_MINUS,  // binary -
  PSA_LT,     // <
  PSA_LTE,    // <=
  PSA_GT,     // >
  PSA_GTE,    // >=
  PSA_EQ,     // ==
  PSA_NEQ,    // !=
  PSA_DOLLAR, // $

  // Nonterminals:
  PSA_HANDLE,
  PSA_E,
  PSA_COND,
  PSA_ARGS,
} PSASymbol_t;

// made by AI and asked me to change PTable[1][1] = '>'
static const char PTable[17][17] = {
    {' ', '>', ' ', ' ', '=', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '<', '<', '<', '<', '=', '<', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', ' '},
    {' ', '>', ' ', ' ', ' ', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', '<', '<', '<', '<', ' ', '<', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', ' '}};

static PSASymbol_t TokenToSym(TokenType_t t, PSASymbol_t top) {
  switch (t) {
  case TOKEN_IDENTIFIER:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
    return PSA_ID;
  case TOKEN_COMMA:
    return PSA_COMMA;
  case TOKEN_LPAR:
    return PSA_LPAR;
  case TOKEN_RPAR:
    return PSA_RPAR;
  case TOKEN_STAR:
    return PSA_MUL;
  case TOKEN_SLASH:
    return PSA_DIV;
  case TOKEN_PLUS:
    if (top == PSA_ID || top == PSA_E || top == PSA_RPAR)
      return PSA_PLUS;
    return PSA_UPLUS;
  case TOKEN_MINUS:
    if (top == PSA_ID || top == PSA_E || top == PSA_RPAR)
      return PSA_MINUS;
    return PSA_UMINUS;
  case TOKEN_LESS:
    return PSA_LT;
  case TOKEN_LESS_EQ:
    return PSA_LTE;
  case TOKEN_GREATER:
    return PSA_GT;
  case TOKEN_GREATER_EQ:
    return PSA_GTE;
  case TOKEN_EQ:
    return PSA_EQ;
  case TOKEN_NOT_EQ:
    return PSA_NEQ;
  default:
    return PSA_DOLLAR;
  }
}

typedef struct PStackItem_t {
  Token_t token;
  PSASymbol_t sym;
  struct PStackItem_t *next;
} PStackItem_t;

typedef struct PStack_t {
  PStackItem_t *top;
  PStackItem_t *afterTerminal;
} PStack_t;

static bool StackInit(PStack_t *stack) {
  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));

  if (newItem == NULL)
    return false;

  newItem->sym = PSA_DOLLAR;
  newItem->token = CreateToken();
  newItem->next = NULL;
  stack->top = newItem;
  stack->afterTerminal = NULL;

  return true;
}

static void StackFree(PStack_t *stack) {
  PStackItem_t *current = stack->top;
  while (current != NULL) {
    PStackItem_t *next = current->next;
    TerminateToken(&current->token);
    free(current);
    current = next;
  }
  stack->top = NULL;
  stack->afterTerminal = NULL;
}

static bool StackPushAfterTerminal(PStack_t *stack) {
  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));

  if (newItem == NULL)
    return false;

  newItem->sym = PSA_HANDLE;
  newItem->token = CreateToken();
  // terminal is on top
  if (stack->afterTerminal == NULL) {
    newItem->next = stack->top;
    stack->top = newItem;
    stack->afterTerminal = newItem;
  } else {
    newItem->next = stack->afterTerminal->next;
    stack->afterTerminal->next = newItem;
    stack->afterTerminal = newItem;
  }
  return true;
}

static bool StackPush(PStack_t *stack, Token_t *token, PSASymbol_t sym) {
  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));

  if (newItem == NULL)
    return false;

  newItem->sym = sym;

  if (sym <= PSA_DOLLAR)
    stack->afterTerminal = NULL;
  else if (stack->afterTerminal == NULL)
    stack->afterTerminal = newItem;

  if (token != NULL) {
    newItem->token = *token;
    token->lexemeS = NULL;
  } else
    newItem->token = CreateToken();

  newItem->next = stack->top;
  stack->top = newItem;

  return true;
}

static PStackItem_t *StackTopTerminal(const PStack_t *stack) {
  return (stack->afterTerminal == NULL) ? stack->top
                                        : stack->afterTerminal->next;
}

static bool Reduce(PStack_t *stack) {
  PStackItem_t *items[MAX_RULE_LENGTH];
  PStackItem_t *current = stack->top;
  int len = 0;

  while (len < MAX_RULE_LENGTH && current != NULL &&
         current->sym != PSA_HANDLE) {
    items[len] = current;
    current = current->next;
    len++;
  }
  if (current == NULL || current->sym != PSA_HANDLE)
    return false;

  PSASymbol_t resultSym = PSA_E;

  switch (len) {
  case 1:
    if (items[0]->sym != PSA_ID)
      return false;
    // Action for: E -> id
    resultSym = PSA_E;
    break;
  case 2:
    if (items[0]->sym != PSA_E ||
        (items[1]->sym != PSA_UMINUS && items[1]->sym != PSA_UPLUS))
      return false;
    // Action for: E -> u+ E / u- E
    resultSym = PSA_E;
    break;
  case 3:
    switch (items[2]->sym) {
    case PSA_LPAR:
      if (items[1]->sym != PSA_E || items[0]->sym != PSA_RPAR)
        return false;

      // Action for: E -> ( E )
      resultSym = PSA_E;
      break;

    case PSA_ID:
      if (items[1]->sym != PSA_LPAR || items[0]->sym != PSA_RPAR)
        return false;

      if (items[2]->token.type != TOKEN_IDENTIFIER)
        return false;

      // Action for: E -> id ( )
      resultSym = PSA_E;
      break;

    case PSA_ARGS:
      if (items[1]->sym != PSA_COMMA || items[0]->sym != PSA_E)
        return false;

      // Action for: ARGS -> ARGS , E
      resultSym = PSA_ARGS;
      break;

    case PSA_E:
      switch (items[1]->sym) {
      case PSA_COMMA:
        if (items[0]->sym != PSA_E)
          return false;
        // Action for: ARGS -> E , E
        resultSym = PSA_ARGS;
        break;

      case PSA_PLUS:
      case PSA_MINUS:
      case PSA_DIV:
      case PSA_MUL:
        if (items[0]->sym != PSA_E)
          return false;
        // Action for: E -> E op E
        resultSym = PSA_E;
        break;

      case PSA_NEQ:
      case PSA_EQ:
      case PSA_GTE:
      case PSA_GT:
      case PSA_LTE:
      case PSA_LT:
        if (items[0]->sym != PSA_E)
          return false;
        // Action for: COND -> E op E
        resultSym = PSA_COND;
        break;

      default:
        return false;
      }
      break;

    default:
      return false;
    }
    break;

  case 4:
    if (items[3]->sym != PSA_ID || items[2]->sym != PSA_LPAR ||
        items[0]->sym != PSA_RPAR)
      return false;

    if (items[3]->token.type != TOKEN_IDENTIFIER)
      return false;

    if (items[1]->sym != PSA_E && items[1]->sym != PSA_ARGS)
      return false;
    // Action for: E -> id ( E ) or E -> id ( ARGS )
    resultSym = PSA_E;
    break;

  default:
    return false;
  }

  stack->top = current->next;
  stack->afterTerminal = NULL;

  for (int i = 0; i < len; i++) {
    TerminateToken(&items[i]->token);
    free(items[i]);
  }
  free(current);
  return StackPush(stack, NULL, resultSym);
}

/* Parses nonterminal expression using preceding parsing
* returns false on error, else true, more info about error in p->errorCode
* should return ASTNode_t in the future
* */
bool ParseExpression(Parser_t *p) {
  PStack_t stack;

  if (!StackInit(&stack)) {
    p->errorCode = ERROR_MEM;
    return false;
  }

  while (true) {
    if (p->current.type == TOKEN_ERROR) {
      if (p->errorCode == ERROR_NOERR) {
        p->errorCode = p->current.error;
      }
      StackFree(&stack);
      return false;
    }

    PSASymbol_t topTerminal = StackTopTerminal(&stack)->sym;
    PSASymbol_t currentSymbol = TokenToSym(p->current.type, stack.top->sym);

    if (topTerminal == PSA_DOLLAR && currentSymbol == PSA_DOLLAR) {
      if ((stack.top->sym != PSA_E && stack.top->sym != PSA_COND) ||
          stack.top->next == NULL || stack.top->next->sym != PSA_DOLLAR) {
        if (p->errorCode == ERROR_NOERR) {
          p->errorCode = ERROR_SYNTAX;
        }
        StackFree(&stack);
        return false;
      }
      StackFree(&stack);
      return true;
    }

    switch (PTable[topTerminal][currentSymbol]) {
    case '<':
      if (!StackPushAfterTerminal(&stack)) {
        p->errorCode = ERROR_MEM;
        StackFree(&stack);
        return false;
      }
    // fallthrough
    case '=':
      if (!StackPush(&stack, &p->current, currentSymbol)) {
        p->errorCode = ERROR_MEM;
        StackFree(&stack);
        return false;
      }
      if (!Advance(p)) {
        StackFree(&stack);
        return false;
      }
      break;

    case '>':
      if (!Reduce(&stack)) {
        if (p->errorCode == ERROR_NOERR)
          p->errorCode = ERROR_SYNTAX;
        StackFree(&stack);
        return false;
      }
      break;

    default:
      if (p->errorCode == ERROR_NOERR)
        p->errorCode = ERROR_SYNTAX;
      StackFree(&stack);
      return false;
    }
  }
}