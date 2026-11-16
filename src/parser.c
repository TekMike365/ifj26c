/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct Parser_t {
  Token_t current;
  Token_t next;
  LayoutFilter_t filter;
  ErrorCode_t errorCode;
} Parser_t;

bool Advance(Parser_t *p);
bool Consume(Parser_t *p, TokenType_t expected);
bool ConsumeId(Parser_t *p, const char *name);
bool ParseRoot(Parser_t *p);
bool ParseProlog(Parser_t *p);
bool ParseProg(Parser_t *p);
bool ParseType(Parser_t *p);
bool ParseFnDef(Parser_t *p);
bool ParseFnBody(Parser_t *p);
bool ParseBlock(Parser_t *p);
bool ParseStatements(Parser_t *p);
bool ParseStatementsC(Parser_t *p);
bool ParseStatement(Parser_t *p);
bool ParseVarDef(Parser_t *p);
bool ParseAssign(Parser_t *p);
bool ParseIfElse(Parser_t *p);
bool ParseWhile(Parser_t *p);
bool ParseParams(Parser_t *p);
bool ParseParamsC(Parser_t *p);
bool ParseParam(Parser_t *p);
bool ParseExpression(Parser_t *p);

bool Consume(Parser_t *p, TokenType_t expected) {
  if (p->current.type != expected) {
    p->errorCode = ERROR_SYNTAX;
    return false;
  }

  if (!Advance(p))
    return false;

  return true;
}
bool ConsumeId(Parser_t *p, const char *name) {
  if (p->current.type != TOKEN_IDENTIFIER || p->current.lexemeS == NULL)
    return false;

  if (strcmp(p->current.lexemeS, name)) {
    p->errorCode = ERROR_SYNTAX;
    return false;
  }

  if (!Advance(p))
    return false;

  return true;
}

bool Advance(Parser_t *p) {
  TerminateToken(&p->current);

  p->current = p->next;

  if (p->current.type == TOKEN_ERROR) {
    p->errorCode = p->current.error;
    return false;
  }

  p->next = GetNextToken(&p->filter);

  return true;
}

bool ParserInit(Parser_t *p, FILE *src) {
  p->errorCode = ERROR_NOERR;

  if (!LayoutFilterInit(&p->filter, src)) {
    p->errorCode = ERROR_MEM;
    return false;
  }

  p->current = GetNextToken(&p->filter);

  if (p->current.type == TOKEN_ERROR) {
    return false;
  }

  p->next = GetNextToken(&p->filter);

  if (p->next.type == TOKEN_ERROR)
    return false;

  return true;
}

void ParserTerminate(Parser_t *p) {
  TerminateToken(&p->current);

  TerminateToken(&p->next);

  LayoutFilterTerminate(&p->filter);
}

bool Parse(FILE *src) {
  Parser_t p;

  if (!ParserInit(&p, src)) {
    return false;
  }
  bool result = ParseRoot(&p);

  ParserTerminate(&p);

  return result;
}

bool ParseRoot(Parser_t *p) {
  if (!ParseProlog(p))
    return false;

  return ParseProg(p);
}

bool ParseProlog(Parser_t *p) {
  if (!Consume(p, TOKEN_KEYWORD_IMPORT))
    return false;

  if (!ConsumeId(p, "ifj26"))
    return false;

  if (!Consume(p, TOKEN_DOT))
    return false;

  if (!Consume(p, TOKEN_STAR))
    return false;

  return Consume(p, TOKEN_NEWLINE);
}

bool ParseType(Parser_t *p) {
  switch (p->current.type) {
  case TOKEN_KEYWORD_INT:
    return Consume(p, TOKEN_KEYWORD_INT);
  case TOKEN_KEYWORD_DOUBLE:
    return Consume(p, TOKEN_KEYWORD_DOUBLE);
  case TOKEN_KEYWORD_BOOLEAN:
    return Consume(p, TOKEN_KEYWORD_BOOLEAN);
  case TOKEN_KEYWORD_UNIT:
    return Consume(p, TOKEN_KEYWORD_UNIT);
  case TOKEN_KEYWORD_STRING:
    return Consume(p, TOKEN_KEYWORD_STRING);
  default:
    return false;
  }
}

bool ParseProg(Parser_t *p) {
  // empty prog
  if (Consume(p, TOKEN_NEWLINE))
    return ParseProg(p);

  if (Consume(p, TOKEN_EOF))
    return true;
  // main before fndef
  if (Consume(p, TOKEN_AT)) {
    if (!ConsumeId(p, "main"))
      return false;

    Consume(p, TOKEN_NEWLINE);
  }
  // fndef
  if (p->current.type == TOKEN_KEYWORD_DEF) {
    if (!ParseFnDef(p))
      return false;
    // prog
    return ParseProg(p);
  }
  return false;
}

bool ParseBlock(Parser_t *p) {
  if (!Consume(p, TOKEN_NEWLINE))
    return false;

  if (!Consume(p, TOKEN_INDENT))
    return false;

  if (!ParseStatements(p))
    return false;

  return Consume(p, TOKEN_DEDENT);
}

bool ParseStatements(Parser_t *p) {
  if (!ParseStatement(p))
    return false;

  return ParseStatementsC(p);
}

bool ParseStatementsC(Parser_t *p) {
  if (Consume(p, TOKEN_SEMICOLON)) {
    if (!ParseStatement(p))
      return false;
    return ParseStatementsC(p);
  } else if (Consume(p, TOKEN_NEWLINE)) {
    if (p->current.type == TOKEN_DEDENT)
      return true;

    if (!ParseStatement(p))
      return false;

    return ParseStatementsC(p);
  }
  return false;
}

bool ParseStatement(Parser_t *p) {
  switch (p->current.type) {
  case TOKEN_KEYWORD_VAR:
  case TOKEN_KEYWORD_VAL:
    return ParseVarDef(p);

  case TOKEN_KEYWORD_IF:
    return ParseIfElse(p);

  case TOKEN_KEYWORD_WHILE:
    return ParseWhile(p);

  case TOKEN_KEYWORD_RETURN:
    if (!Consume(p, TOKEN_KEYWORD_RETURN))
      return false;

    if (p->current.type == TOKEN_NEWLINE || p->current.type == TOKEN_SEMICOLON)
      return true;

    return ParseExpression(p);

  case TOKEN_IDENTIFIER:
    if (p->next.type == TOKEN_EQ)
      return ParseAssign(p);
    // if not <ASSIGN> continue with <EXPRESSION>
  default:
    return ParseExpression(p);
  }
}

bool ParseVarDef(Parser_t *p) {
  if (Consume(p, TOKEN_KEYWORD_VAL)) {
    // semantic action
  } else if (Consume(p, TOKEN_KEYWORD_VAR)) {
    // semantic action
  } else
    return false;

  if (!Consume(p, TOKEN_IDENTIFIER))
    return false;

  if (Consume(p, TOKEN_COLON))
    if (!ParseType(p))
      return false; // not in condition means that type is optional

  if (!Consume(p, TOKEN_EQ))
    return false;

  if (!ParseExpression(p))
    return false;

  return true;
}

bool ParseAssign(Parser_t *p) {
  if (!Consume(p, TOKEN_IDENTIFIER))
    return false;

  if (!Consume(p, TOKEN_EQ))
    return false;

  if (!ParseExpression(p))
    return false;

  return true;
}

bool ParseFnDef(Parser_t *p) {
  if (!Consume(p, TOKEN_KEYWORD_DEF))
    return false;

  if (!Consume(p, TOKEN_IDENTIFIER))
    return false;

  if (!Consume(p, TOKEN_LPAR))
    return false;

  if (!ParseParams(p))
    return false;

  if (!Consume(p, TOKEN_RPAR))
    return false;

  if (!Consume(p, TOKEN_COLON)) // TODO check if ':' is mandatory
    return false;

  if (!ParseType(p)) // TODO something with types also if mandatory
    return false;

  if (!Consume(p, TOKEN_EQ))
    return false;

  return ParseFnBody(p);
}

bool ParseFnBody(Parser_t *p) {
  if (p->current.type == TOKEN_NEWLINE)
    return ParseBlock(p);

  return ParseExpression(p);
}

bool ParseIfElse(Parser_t *p) {
  if (!Consume(p, TOKEN_KEYWORD_IF))
    return false;

  if (!ParseExpression(p))
    return false;

  if (!Consume(p, TOKEN_KEYWORD_THEN))
    return false;

  if (!ParseBlock(p))
    return false;

  if (Consume(p, TOKEN_KEYWORD_ELSE))
    return ParseBlock(p);

  return true;
}

bool ParseWhile(Parser_t *p) {
  if (!Consume(p, TOKEN_KEYWORD_WHILE))
    return false;

  if (!ParseExpression(p))
    return false;

  if (!Consume(p, TOKEN_KEYWORD_DO))
    return false;

  return ParseBlock(p);
}

bool ParseParam(Parser_t *p) {
  if (!Consume(p, TOKEN_IDENTIFIER))
    return false;

  if (!Consume(p, TOKEN_COLON))
    return false;

  return ParseType(p);
}

bool ParseParams(Parser_t *p) {
  if (p->current.type == TOKEN_RPAR)
    return true;

  if (!ParseParam(p))
    return false;

  return ParseParamsC(p);
}

bool ParseParamsC(Parser_t *p) {
  if (p->current.type == TOKEN_RPAR)
    return true;

  if (!Consume(p, TOKEN_COMMA))
    return false;

  if (!ParseParam(p))
    return false;

  return ParseParamsC(p);
}
// begin PSA:
// made by AI

#define MAX_RULE_LENGTH 4

typedef enum {
  PSA_ID = 0, // id (int, double, string, true, false, null)
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

PSASymbol_t TokenToSym(TokenType_t t) {
  switch (t) {
  case TOKEN_IDENTIFIER:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
    return PSA_ID;
  case TOKEN_COMMA:
    return PSA_COMMA;
  case TOKEN_UPLUS:
    return PSA_UPLUS;
  case TOKEN_UMINUS:
    return PSA_UMINUS;
  case TOKEN_LPAR:
    return PSA_LPAR;
  case TOKEN_RPAR:
    return PSA_RPAR;
  case TOKEN_STAR:
    return PSA_MUL; // *
  case TOKEN_SLASH:
    return PSA_DIV; // /
  case TOKEN_PLUS:
    return PSA_PLUS; // binárne +
  case TOKEN_MINUS:
    return PSA_MINUS; // binárne -
  case TOKEN_LESS:
    return PSA_LT; // <
  case TOKEN_LESS_EQ:
    return PSA_LTE; // <=
  case TOKEN_GREATER:
    return PSA_GT; // >
  case TOKEN_GREATER_EQ:
    return PSA_GTE; // >=
  case TOKEN_EQ:
    return PSA_EQ; // ==
  case TOKEN_NOT_EQ:
    return PSA_NEQ; // !=
  default:
    return PSA_DOLLAR; // $
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

bool StackInit(PStack_t *stack) {
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

void StackFree(PStack_t *stack) {
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

bool StackPushAfterTerminal(PStack_t *stack) {
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

bool StackPush(PStack_t *stack, Token_t *token, PSASymbol_t sym) {
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

PStackItem_t *StackTopTerminal(const PStack_t *stack) {
  return (stack->afterTerminal == NULL) ? stack->top
                                        : stack->afterTerminal->next;
}

bool Reduce(PStack_t *stack) {
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

// Parses expression using PSA
bool ParseExpression(Parser_t *p) {
  PStack_t stack;

  if (!StackInit(&stack)) {
    p->errorCode = ERROR_MEM;
    return false;
  }

  while (true) {
    PSASymbol_t topTerminal = StackTopTerminal(&stack)->sym;
    PSASymbol_t currentSymbol = TokenToSym(p->current.type);

    if (topTerminal == PSA_DOLLAR && currentSymbol == PSA_DOLLAR) {
      if ((stack.top->sym != PSA_E && stack.top->sym != PSA_COND) ||
          stack.top->next == NULL || stack.top->next->sym != PSA_DOLLAR) {
        if (p->errorCode == ERROR_NOERR) {
          p->errorCode = ERROR_SYNTAX;
          StackFree(&stack);
          return false;
        }
      }
      StackFree(&stack);
      return true;
    }

    switch (PTable[topTerminal][currentSymbol]) {
    case '<':
      if (!StackPushAfterTerminal(&stack)) {
        StackFree(&stack);
        return false;
      }
    case '=':
      if (!StackPush(&stack, &p->current, TokenToSym(p->current.type))) {
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