/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "as_tree.h"
#include "expression.c"
#include "lexer.h"
#include "parser_internal.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool Advance(Parser_t *p);
bool Consume(Parser_t *p, TokenType_t expected);
bool ConsumeId(Parser_t *p, const char *name);

static ASTNode_t *ParseRoot(Parser_t *p);

static bool ParseProlog(Parser_t *p);
static bool ParseProg(Parser_t *p, ASTList_t *list);
static bool ParseType(Parser_t *p, DataType_t *out);
static ASTNode_t *ParseFnDef(Parser_t *p);
static ASTNode_t *ParseFnBody(Parser_t *p);
static ASTNode_t *ParseBlock(Parser_t *p);

static bool ParseStatements(Parser_t *p, ASTList_t *list);
static bool ParseStatementsC(Parser_t *p, ASTList_t *list);
static ASTNode_t *ParseStatement(Parser_t *p);
static ASTNode_t *ParseVarDef(Parser_t *p);
static ASTNode_t *ParseAssign(Parser_t *p);
static ASTNode_t *ParseIfElse(Parser_t *p);
static ASTNode_t *ParseWhile(Parser_t *p);

static bool ParseParams(Parser_t *p, ASTList_t *list);
static bool ParseParamsC(Parser_t *p, ASTList_t *list);
static ASTNode_t *ParseParam(Parser_t *p);

/*Sets p->errorCode as either ERROR_LEX or ERROR_SYNTAX depending on the token
 * p->current
 * */
static void SetError(Parser_t *p) {
  if (p->current.type == TOKEN_ERROR)
    p->errorCode = p->current.error;
  else
    p->errorCode = ERROR_SYNTAX;
}

/* consumes token p->current if p->current matches with 'expected'
 * sets error if unable to Consume or Advance
 * returns true if consumed, else false
 * */
static bool Consume(Parser_t *p, TokenType_t expected) {
  if (p->current.type != expected) {
    SetError(p);
    return false;
  }
  if (!Advance(p))
    return false;

  return true;
}

static bool GetIdentifier(Parser_t *p, char **out) {
  if (p->current.type != TOKEN_IDENTIFIER || p->current.lexemeS == NULL) {
    SetError(p);
    return false;
  }
  *out = p->current.lexemeS;
  p->current.lexemeS = NULL;
  return Advance(p);
}

/* consumes token p->current if is TOKEN_IDENTIFIER and lexemeS matches with
 * expected name
 * sets error if unable to Consume or Advance
 * returns true if consumed, else false
 * */
static bool ConsumeId(Parser_t *p, const char *name) {
  if (p->current.type != TOKEN_IDENTIFIER || p->current.lexemeS == NULL) {
    SetError(p);
    return false;
  }

  if (strcmp(p->current.lexemeS, name)) {
    SetError(p);
    return false;
  }

  if (!Advance(p))
    return false;

  return true;
}

/*loads token from p->next to p->current and loads next token from lexer into
 * p->next if new p->current is ERROR_TOKEN, writes error code to p->errorCode
 * returns true if no Error, else false
 * */
bool Advance(Parser_t *p) {
  TerminateToken(&p->current);

  p->current = p->next;
  p->next = CreateToken();

  if (p->current.type == TOKEN_ERROR) {
    p->errorCode = p->current.error;
    return false;
  }

  p->next = GetNextToken(&p->filter);

  return true;
}

static bool ParserInit(Parser_t *p, FILE *src) {
  p->errorCode = ERROR_NOERR;
  p->current = CreateToken();
  p->next = CreateToken();

  if (!LayoutFilterInit(&p->filter, src)) {
    p->errorCode = ERROR_MEM;
    return false;
  }

  p->current = GetNextToken(&p->filter);

  if (p->current.type == TOKEN_ERROR) {
    p->errorCode = p->current.error;
    ParserTerminate(p);
    return false;
  }

  p->next = GetNextToken(&p->filter);

  return true;
}

static void ParserTerminate(Parser_t *p) {
  TerminateToken(&p->current);

  TerminateToken(&p->next);

  LayoutFilterTerminate(&p->filter);
}

/* Parses source file
 * creates complete AST of file (whitout checking types) if successful, else
 * NULL
 *  returns enum ErrorCode_t
 * */
ErrorCode_t Parse(FILE *src, ASTNode_t **ProgramTree) {
  *ProgramTree = NULL;

  Parser_t p;

  if (!ParserInit(&p, src))
    return p.errorCode;

  *ProgramTree = ParseRoot(&p);

  ParserTerminate(&p);

  if (*ProgramTree != NULL)
    return ERROR_NOERR;

  return p.errorCode != ERROR_NOERR ? p.errorCode : ERROR_SYNTAX;
}

static ASTNode_t *ParseRoot(Parser_t *p) {
  if (!ParseProlog(p))
    return NULL;
  ASTNode_t *node = ASTCreate(NODE_PROGRAM, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!ParseProg(p, node->program.items)) {
    ASTFree(node);
    return NULL;
  }
  return node;
}

static bool ParseProlog(Parser_t *p) {
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

bool ParseType(Parser_t *p, DataType_t *out) {
  switch (p->current.type) {
  case TOKEN_KEYWORD_INT:
    *out = TYPE_INT;
    break;

  case TOKEN_KEYWORD_DOUBLE:
    *out = TYPE_DOUBLE;
    break;

  case TOKEN_KEYWORD_BOOLEAN:
    *out = TYPE_BOOLEAN;
    break;

  case TOKEN_KEYWORD_UNIT:
    *out = TYPE_UNIT;
    break;

  case TOKEN_KEYWORD_STRING:
    *out = TYPE_STRING;
    break;

  default:
    SetError(p);
    return false;
  }
  if (!Advance(p))
    return false;
  return true;
}

static bool ParseProg(Parser_t *p, ASTList_t *list) {
  while (true) {
    while (p->current.type == TOKEN_NEWLINE)
      if (!Consume(p, TOKEN_NEWLINE))
        return false;

    if (p->current.type == TOKEN_EOF)
      return true;

    // main before fndef
    if (p->current.type == TOKEN_AT) {
      if (!Consume(p, TOKEN_AT))
        return false;
      if (!ConsumeId(p, "main"))
        return false;

      while (p->current.type == TOKEN_NEWLINE)
        if (!Consume(p, TOKEN_NEWLINE))
          return false;
    }
    // fndef
    if (!ASTAppend(list, ParseFnDef(p)))
      return false;
  }
}

static ASTNode_t *ParseBlock(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_BLOCK, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }
  if (!Consume(p, TOKEN_NEWLINE))
    goto failure;

  if (!Consume(p, TOKEN_INDENT))
    goto failure;

  if (!ParseStatements(p, &node->block))
    goto failure;

  if (!Consume(p, TOKEN_DEDENT))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static bool ParseStatements(Parser_t *p, ASTList_t *list) {
  if (!ASTAppend(list, ParseStatement(p)))
    return false;

  return ParseStatementsC(p, list);
}

static bool ParseStatementsC(Parser_t *p, ASTList_t *list) {
  while (true) {
    if (p->current.type == TOKEN_SEMICOLON) {
      if (!Consume(p, TOKEN_SEMICOLON))
        return false;
      if (!ASTAppend(list, ParseStatement(p)))
        return false;

    } else if (p->current.type == TOKEN_NEWLINE) {
      if (!Consume(p, TOKEN_NEWLINE))
        return false;
      if (p->current.type == TOKEN_DEDENT)
        return true;

      if (!ASTAppend(list, ParseStatement(p)))
        return false;
    } else {
      return false;
    }
  }
}

static ASTNode_t *ParseReturn(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_RETURN, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }
  if (!Consume(p, TOKEN_KEYWORD_RETURN))
    goto failure;

  if (p->current.type != TOKEN_NEWLINE && p->current.type != TOKEN_SEMICOLON)
    if (!(node->ret.value = ParseExpression(p)))
      goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseStatement(Parser_t *p) {
  switch (p->current.type) {
  case TOKEN_KEYWORD_VAR:
  case TOKEN_KEYWORD_VAL:
    return ParseVarDef(p);

  case TOKEN_KEYWORD_IF:
    return ParseIfElse(p);

  case TOKEN_KEYWORD_WHILE:
    return ParseWhile(p);

  case TOKEN_KEYWORD_RETURN:
    return ParseReturn(p);

  case TOKEN_IDENTIFIER:
    if (p->next.type == TOKEN_EQ)
      return ParseAssign(p);
    return ParseExpression(p);

  default:
    return ParseExpression(p);
  }
}

static ASTNode_t *ParseVarDef(Parser_t *p) {
  ASTNode_t *node = NULL;
  if (p->current.type == TOKEN_KEYWORD_VAL) {
    if (!Consume(p, TOKEN_KEYWORD_VAL))
      return NULL;
    node = ASTCreate(NODE_VALDEF, p->current.pos);
  } else if (p->current.type == TOKEN_KEYWORD_VAR) {
    if (!Consume(p, TOKEN_KEYWORD_VAR))
      return NULL;
    node = ASTCreate(NODE_VARDEF, p->current.pos);
  }

  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!GetIdentifier(p, &node->varDef.name))
    goto failure;

  if (p->current.type == TOKEN_COLON) {
    if (!Consume(p, TOKEN_COLON))
      goto failure;
    if (!ParseType(p, &node->varDef.declType))
      goto failure;
  }

  if (!Consume(p, TOKEN_EQ))
    goto failure;

  if (!(node->varDef.init = ParseExpression(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseAssign(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_ASSIGN, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!GetIdentifier(p, &node->assign.name))
    goto failure;

  if (!Consume(p, TOKEN_EQ))
    goto failure;

  if (!(node->assign.value = ParseExpression(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseFnDef(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_FNDEF, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!Consume(p, TOKEN_KEYWORD_DEF))
    goto failure;

  if (!GetIdentifier(p, &node->fnDef.name))
    goto failure;

  if (!Consume(p, TOKEN_LPAR))
    goto failure;

  if (!ParseParams(p, &node->fnDef.params))
    goto failure;

  if (!Consume(p, TOKEN_RPAR))
    goto failure;

  if (p->current.type == TOKEN_COLON) {
    if (!Consume(p, TOKEN_COLON))
      goto failure;

    if (!(ParseType(p, &node->fnDef.declType)))
      goto failure;
  }

  if (!Consume(p, TOKEN_EQ))
    goto failure;

  if (!(node->fnDef.body = ParseFnBody(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseFnBody(Parser_t *p) {
  if (p->current.type == TOKEN_NEWLINE)
    return ParseBlock(p);

  // one-line functions
  ASTNode_t *node = ASTCreate(NODE_BLOCK, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  ASTNode_t *expr = ParseExpression(p);
  if (expr == NULL) {
    ASTFree(node);
    return NULL;
  }

  if (!ASTAppend(&node->block, expr)) {
    ASTFree(node);
    ASTFree(expr);
    return NULL;
  }

  return node;
}

static ASTNode_t *ParseIfElse(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_IF, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!Consume(p, TOKEN_KEYWORD_IF))
    goto failure;

  if (!(node->ifElse.cond = ParseExpression(p)))
    goto failure;

  if (!Consume(p, TOKEN_KEYWORD_THEN))
    goto failure;

  if (!(node->ifElse.thenBlock = ParseBlock(p)))
    goto failure;

  if (p->current.type == TOKEN_KEYWORD_ELSE) {
    if (!Consume(p, TOKEN_KEYWORD_ELSE))
      goto failure;
    if (!(node->ifElse.elseBlock = ParseBlock(p)))
      goto failure;
  }
  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseWhile(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_WHILE, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  if (!Consume(p, TOKEN_KEYWORD_WHILE))
    goto failure;

  if (!(node->whileDo.cond = ParseExpression(p)))
    goto failure;

  if (!Consume(p, TOKEN_KEYWORD_DO))
    goto failure;

  if (!(node->whileDo.body = ParseBlock(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static ASTNode_t *ParseParam(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_PARAM, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }
  if (!GetIdentifier(p, &node->param.name))
    goto failure;

  if (!Consume(p, TOKEN_COLON))
    goto failure;

  if (!(ParseType(p, node->param.declType)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

static bool ParseParams(Parser_t *p, ASTList_t *list) {
  if (p->current.type == TOKEN_RPAR)
    return true;

  if (!ASTAppend(list, ParseParam(p)))
    return false;

  return ParseParamsC(p, list);
}

static bool ParseParamsC(Parser_t *p, ASTList_t *list) {
  while (p->current.type != TOKEN_RPAR) {
    if (!Consume(p, TOKEN_COMMA))
      return false;

    if (p->current.type == TOKEN_RPAR) { // trailing comma
      if (p->current.nlBefore)
        return true;
      SetError(p);
      return false;
    }

    if (!ASTAppend(list, ParseParam(p)))
      return false;
  }
  return true;
}
