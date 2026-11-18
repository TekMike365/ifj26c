/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */

#include "parser_internal.h"
#include <string.h>

bool Advance(Parser_t *p);
static bool Consume(Parser_t *p, TokenType_t expected);
static bool ConsumeId(Parser_t *p, const char *name);

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

static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind) {
  ASTNode_t *node = ASTCreate(kind, p->current.pos);

  if (node == NULL)
    p->errorCode = ERROR_MEM;

  return node;
}

static bool AppendNode(Parser_t *p, ASTList_t *list, ASTNode_t *node) {
  if (node == NULL) {
    return false;
  }
  if (!ASTAppend(list, node)) {
    ASTFree(node);
    p->errorCode = ERROR_MEM;
    return false;
  }
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

// terminates tokens and layoutfilter
static void ParserTerminate(Parser_t *p) {
  TerminateToken(&p->current);

  TerminateToken(&p->next);

  LayoutFilterTerminate(&p->filter);
}

// temporary layoutfilter
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

// <root> = <prolog> <prog>
static ASTNode_t *ParseRoot(Parser_t *p) {
  if (!ParseProlog(p))
    return NULL;

  ASTNode_t *node = NewNode(p, NODE_PROGRAM);
  if (node == NULL)
    return NULL;

  if (!ParseProg(p, &node->program)) {
    ASTFree(node);
    return NULL;
  }
  return node;
}

// <prolog> = import ifj26 . * LF
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

// <type> = : type
// type is one of (int, double, boolean, unit, string)
static bool ParseType(Parser_t *p, DataType_t *out) {
  if (!Consume(p, TOKEN_COLON))
    return false;

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

// <main> = @ main LF*
// different from docs, essentialy deletes all newlines after @ main
static bool ParseMain(Parser_t *p) {
  if (!Consume(p, TOKEN_AT))
    return false;

  if (!ConsumeId(p, "main"))
    return false;

  while (p->current.type == TOKEN_NEWLINE)
    if (!Consume(p, TOKEN_NEWLINE))
      return false;

  return true;
}

// if LF:   <prog> = LF <prog>
// if EOF:  <prog> = {}
// if @:    <prog> = <main> <fndef> <prog>
// else:    <prog> = <fndef> <prog>
static bool ParseProg(Parser_t *p, ASTList_t *list) {
  while (true) {
    bool isMain = false;

    while (p->current.type == TOKEN_NEWLINE)
      if (!Consume(p, TOKEN_NEWLINE))
        return false;

    if (p->current.type == TOKEN_EOF)
      return true;

    // main before fndef
    if (p->current.type == TOKEN_AT) {
      if (!ParseMain(p))
        return false;

      isMain = true;
    }

    ASTNode_t *node = ParseFnDef(p);
    if (node == NULL)
      return false;

    node->fnDef.isMain = isMain;
    if (!AppendNode(p, list, node))
      return false;
  }
}

// <block> = LF INDENT <stmnts> DEDENT
static ASTNode_t *ParseBlock(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_BLOCK);
  if (node == NULL)
    return NULL;

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

// <stmnts> = <stmnt> <stmntsC>
static bool ParseStatements(Parser_t *p, ASTList_t *list) {
  if (!AppendNode(p, list, ParseStatement(p)))
    return false;

  return ParseStatementsC(p, list);
}

// if ';':            <stmtsC>  = ;  <stmnt> <stmntsC> // implemented with loop
// if LF not DEDENT:  <stmntsC> = LF <stmnt> <stmntsC> // implemented with loop
// if LF and DEDENT:  <stmntsC> = LF epsilon
static bool ParseStatementsC(Parser_t *p, ASTList_t *list) {
  while (true) {
    if (p->current.type == TOKEN_SEMICOLON) {
      if (!Consume(p, TOKEN_SEMICOLON))
        return false;
      if (!AppendNode(p, list, ParseStatement(p)))
        return false;

    } else if (p->current.type == TOKEN_NEWLINE) {
      if (!Consume(p, TOKEN_NEWLINE))
        return false;
      if (p->current.type == TOKEN_DEDENT)
        return true;

      if (!AppendNode(p, list, ParseStatement(p)))
        return false;
    } else {
      SetError(p);
      return false;
    }
  }
}

// if LF:  <return> = return;
// if ';': <return> = return;
// if EOF: <return> = return;
// else:   <return> = return <expr>
static ASTNode_t *ParseReturn(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_RETURN);
  if (node == NULL)
    return NULL;

  if (!Consume(p, TOKEN_KEYWORD_RETURN))
    goto failure;

  if (p->current.type != TOKEN_NEWLINE && p->current.type != TOKEN_SEMICOLON &&
      p->current.type != TOKEN_EOF)
    if (!(node->ret.value = ParseExpression(p)))
      goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// if VAR/VAL:      <stmnt> = <vardef>
// if IF:           <stmnt> = <ifelse>
// if WHILE:        <stmnt> = <while>
// if RETURN:       <stmnt> = <return>
// if ID, next EQ:  <stmnt> = <assing>
// else:            <stmnt> = <expr>
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

// if TOKEN_VAR: <vardef> = var id <type>? = <expr>
// if TOKEN_VAL: <vardef> = val id <type>? = <expr>
static ASTNode_t *ParseVarDef(Parser_t *p) {
  ASTKind_t kind;

  if (p->current.type == TOKEN_KEYWORD_VAL)
    kind = NODE_VALDEF;
  else if (p->current.type == TOKEN_KEYWORD_VAR)
    kind = NODE_VARDEF;
  else {
    SetError(p);
    return NULL;
  }

  ASTNode_t *node = NewNode(p, kind);
  if (node == NULL)
    return NULL;

  if (!GetIdentifier(p, &node->varDef.name))
    goto failure;

  if (p->current.type == TOKEN_COLON) {
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

// <assign> = id = <expr>
static ASTNode_t *ParseAssign(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_ASSIGN);
  if (node == NULL)
    return NULL;

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

// <fndef> = def id ( <params> ) <type?> = <fnbody>
static ASTNode_t *ParseFnDef(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_FNDEF);
  if (node == NULL)
    return NULL;

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

  if (p->current.type == TOKEN_COLON)
    if (!(ParseType(p, &node->fnDef.declType)))
      goto failure;

  if (!Consume(p, TOKEN_EQ))
    goto failure;

  if (!(node->fnDef.body = ParseFnBody(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// if LF: <fnbody> = <block>
// else: <fnbody> = block(<expr>)
// bock(<expr>) means that fnbody contains only expression
static ASTNode_t *ParseFnBody(Parser_t *p) {
  if (p->current.type == TOKEN_NEWLINE)
    return ParseBlock(p);

  // one-line functions
  ASTNode_t *node = NewNode(p, NODE_BLOCK);
  if (node == NULL)
    return NULL;

  if (!AppendNode(p, &node->block, ParseExpression(p))) {
    ASTFree(node);
    return NULL;
  }

  return node;
}

// <ifelse> = if <expr> then <block> [<else> <block>?]
static ASTNode_t *ParseIfElse(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_IF);
  if (node == NULL)
    return NULL;

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

// <while> = while <expr> do <block>
static ASTNode_t *ParseWhile(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_WHILE);
  if (node == NULL)
    return NULL;

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

// <param> = id <type?>
static ASTNode_t *ParseParam(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_PARAM);
  if (node == NULL)
    return NULL;

  if (!GetIdentifier(p, &node->param.name))
    goto failure;

  if (p->current.type == TOKEN_COLON)
    if (!(ParseType(p, &node->param.declType)))
      goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// if ')': <params> = epsilon
// else:          <params> = <param> <paramsC>
static bool ParseParams(Parser_t *p, ASTList_t *list) {
  if (p->current.type == TOKEN_RPAR)
    return true;

  if (!AppendNode(p, list, ParseParam(p)))
    return false;

  return ParseParamsC(p, list);
}

// if ')':  <paramsC> = epsilon
// else:    <paramsC> = , <param> <paramsC>
//          <paramsC> = ,     <- if ')' follows (trailing comma)
static bool ParseParamsC(Parser_t *p, ASTList_t *list) {
  while (p->current.type != TOKEN_RPAR) {
    if (!Consume(p, TOKEN_COMMA))
      return false;

    if (p->current.type == TOKEN_RPAR) {
      if (p->current.nlBefore)
        return true;
      SetError(p);
      return false;
    }

    if (!AppendNode(p, list, ParseParam(p)))
      return false;
  }
  return true;
}
