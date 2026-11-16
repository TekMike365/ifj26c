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

  return ParseType(p); // TODO parse Type
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

// bool stack_push_after_top_terminal(char operator) {} // TODO

// bool stack_push(Token_t token) {} // TODO

// Token_t stack_top_terminal() {} // TODO

// // Parses expression using PSA
// bool ParseExpression(Parser_t *p) {
//   char *stack;
//   char **PSATable;
//   while (stack != "$E" &&
//          p->current.type !=
//              TOKEN_KEYWORD_TYPE) { // TOK_PSA_END for example: then, do but
//                                    // beware of New Lines if in ()
//     Token_t top_t = stack_top_terminal(); // Whats on the top?
//     switch (PSATable[top_t.type][p->current.type]) {
//     case '<':
//       stack_push_after_top_terminal('<');
//     case '=':
//       stack_push(p->current);
//       Advance(p);
//       break;
//     case '>':
//       // find rule between < > on stack; if ok,
//       // reduce and perform semantic action!
//       break;
//     default:
//       // error
//     }
//   }
// }