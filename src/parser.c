/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "error.h"
#include "parser_internal.h"
#include <stdio.h>
#include <string.h>

/* helper functions */
static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind);
static bool AppendNode(Parser_t *p, ASTList_t *list, ASTNode_t *node);
static bool GetIdentifier(Parser_t *p, char **out);
static bool Consume(Parser_t *p, TokenType_t expected);
static bool ConsumeId(Parser_t *p, const char *name);
static bool ParserInit(Parser_t *p);

/* recursive parser functions */
static ASTNode_t *ParseRoot(Parser_t *p);
static bool ParseProlog(Parser_t *p);
static bool ParseProg(Parser_t *p, ASTList_t *list);
static bool ParseType(Parser_t *p, DataType_t *out);
static ASTNode_t *ParseFnDef(Parser_t *p);
static ASTNode_t *ParseFnBody(Parser_t *p);
static ASTNode_t *ParseBlock(Parser_t *p);
static bool ParseStatements(Parser_t *p, ASTList_t *list);
static ASTNode_t *ParseStatement(Parser_t *p);
static ASTNode_t *ParseVarDef(Parser_t *p);
static ASTNode_t *ParseAssign(Parser_t *p);
static ASTNode_t *ParseIfElse(Parser_t *p);
static ASTNode_t *ParseWhile(Parser_t *p);
static ASTNode_t *ParseParam(Parser_t *p);
static bool ParseParams(Parser_t *p, ASTList_t *list);
static bool ParseParamsC(Parser_t *p, ASTList_t *list);

/* Parses an IFJ26 source program from stdin and constructs its Abstract syntax
 * tree. Combines top-down recursive descent for program structure and
 * precedence syntax analysis (PSA) for expressions and funcalls.
 *
 * Does not check var/function declaration types or scopes. On Syntax Error
 * Formatted messages are printed  to stderr. Cleanup of all resources is self
 * performed.
 *
 * Returns:
 *  - Root ASTNode_t * (NODE_PROGRAM) and *errOut = ERROR_NOERR on success.
 *  - NULL on failure, with *errOut set to:
 *     - ERROR_LEX      - lexical error
 *     - ERROR_SYNTAX   - syntactic error
 *     - ERROR_INTERNAL - out of memory or internal error
 * */
ASTNode_t *Parse(ErrorCode_t *errOut) {
  if (errOut == NULL)
    return NULL;

  Parser_t p;

  if (!ParserInit(&p)) {
    *errOut = p.errorCode;
    return NULL;
  }
  ASTNode_t *node = ParseRoot(&p);

  PPDelete(p.pp);

  *errOut = p.errorCode;

  if (node != NULL)
    return node;

  *errOut = (p.errorCode == ERROR_NOERR) ? ERROR_SEM_UNDEF : p.errorCode;
  return NULL;
}

/* consumes token p->current if p->current matches with 'expected'
 * sets error if unable to Consume or Advance
 * returns true if consumed, else false
 * */
static bool Consume(Parser_t *p, TokenType_t expected) {
  if (p->current.type != expected) {
    /* write error detail */
    char detail[128];
    snprintf(detail, sizeof(detail), "Expected %s, got %s",
             TokenToString(expected), TokenToString(p->current.type));

    SyntaxError(p, detail);

    return false;
  }

  if (!Advance(p))
    return false;

  return true;
}

/* Error handling wrapper funtion */
static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind) {
  ASTNode_t *node = ASTCreate(kind, p->current.pos);

  if (node == NULL) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
  }
  return node;
}

/* Error handling wrapper funtion */
static bool AppendNode(Parser_t *p, ASTList_t *list, ASTNode_t *node) {
  if (node == NULL)
    return false;

  if (!ASTAppend(list, node)) {
    ASTFree(node);
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
    return false;
  }
  return true;
}

/* doplicates name from current token
 * returns false and sets error if failed, else true
 */
static bool GetIdentifier(Parser_t *p, char **out) {
  if (p->current.type != TOKEN_IDENTIFIER || p->current.lexemeS == NULL) {
    SyntaxError(p, "Invalid Identifier");
    return false;
  }
  *out = StrDup(p->current.lexemeS);
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
    return false;
  }

  if (strcmp(p->current.lexemeS, name)) {
    return false;
  }

  if (!Advance(p))
    return false;

  return true;
}

// temporary layoutfilter
static bool ParserInit(Parser_t *p) {
  p->errorCode = ERROR_NOERR;
  p->pp = PPCreate();

  if (p->pp == NULL) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
    return false;
  }

  p->current = PPNextToken(p->pp);

  if (p->current.type == TOKEN_ERROR) {
    p->errorCode = p->current.error;
    PPDelete(p->pp);
    return false;
  }
  p->lastValidPos = p->current.pos;
  p->next = PPNextToken(p->pp);

  return true;
}

/* <root> = <prolog> <prog> */
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

  if (!ConsumeId(p, "ifj26")) {
    SyntaxError(p, "Expected: \"ifj26.*\"");
    return false;
  }

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
    SyntaxError(p, "Invalid data type");
    return false;
  }
  if (!Advance(p))
    return false;
  return true;
}

// <main> = @ main LF*
// different from docs - but same, essentialy deletes all newlines after @ main
static bool ParseMain(Parser_t *p) {
  if (!Consume(p, TOKEN_AT))
    return false;

  if (!ConsumeId(p, "main")) {
    SyntaxError(p, "Expected \"main\"");
    return false;
  }
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
//
// if DEDENT:            <stmntsC> = epsilon
// if LF, next DEDENT:   <stmntsC> = LF
// if LF:                <stmntsC> = LF <stmnts>
// if ';':               <stmntsC> = ; <stmnts>
//
// if <stmnt> was IF or WHILE:
//                       <stmntsC> = <stmnts>
static bool ParseStatements(Parser_t *p, ASTList_t *list) {
  while (true) {
    /* If and While stattement blocks are indented */
    bool isIndented = p->current.type == TOKEN_KEYWORD_IF ||
                      p->current.type == TOKEN_KEYWORD_WHILE;

    if (!AppendNode(p, list, ParseStatement(p)))
      return false;

    switch (p->current.type) {
    case TOKEN_SEMICOLON:
      if (!Consume(p, TOKEN_SEMICOLON))
        return false;
      continue;

    case TOKEN_NEWLINE:
      if (!Consume(p, TOKEN_NEWLINE))
        return false;

      if (p->current.type == TOKEN_DEDENT)
        return true;
      continue;

    case TOKEN_DEDENT:
      return true;

    default:
      if (isIndented)
        continue;

      SyntaxError(p, "Invalid end of body");
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
    SyntaxError(p, "Expected var/val definiton");
    return NULL;
  }
  if (!Advance(p)) {
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

// <fndef> = def id ( <params>? ) <type?> = <fnbody>
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

  if (p->current.type != TOKEN_RPAR)
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
// else: <fnbody> = block(<expr>) LF
// This means that fnbody is block with one expression
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
  if (p->current.type == TOKEN_EOF)
    return node;

  if (!Consume(p, TOKEN_NEWLINE)) {
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

  if (node->ifElse.cond != NODE_COND)
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

  if (node->whileDo.cond != NODE_COND)
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
// for id <-
//

static ASTNode_t *ParseForBody(Parser_t *p) {
  ASTNode_t *root = NewNode(p, NODE_BLOCK);
  if (root == NULL)
    return NULL;
  ASTNode_t *nested = root;

  while (true) {
    switch (p->current.type) {

    case TOKEN_KEYWORD_IF:
      if (!Consume(p, TOKEN_KEYWORD_IF))
        goto failure;
      /* Create new node IF */
      if (root == NULL)
        root = NewNode(p, NODE_IF);

      if (!AppendNode(p, &nested->block.items, NewNode(p, NODE_IF)))
        goto failure;

      /* move into the block of the node IF */
      nested = nested->block.items[0];
      if (!(nested->ifElse.cond = ParseExpression(p)))
        goto failure;
      break;

    case TOKEN_IDENTIFIER:

      if (root == NULL)
        return ParseLoop(p);
      /* Create nested loop inside the block */
      if (!AppendNode(p, &nested->block.items, ParseLoop(p)))
        goto failure;
      return root;

    case TOKEN_KEYWORD_DO:
      if (!Consume(p, TOKEN_KEYWORD_DO))
        goto failure;

      if (root == NULL)
        return ParseBlock(p);

      return AppendNode(p, &nested->block.items, ParseBlock(p));

    default:
      goto failure;
    }
  }
failure:
  ASTFree(nested);
  return NULL;
}

#define TOKEN_KEYWORD_UNTIL TOKEN_AMPERSAND
#define TOKEN_KEYWORD_TO TOKEN_AND

static ASTNode_t *ParseLoop(Parser_t *p) {
  ASTNode_t *nodeFor = NewNode(p, NODE_FOR);
  if (nodeFor == NULL)
    return NULL;

  ASTNode_t *nodeGen = NewNode(p, NODE_GEN);
  if (nodeGen == NULL)
    goto failure;

  nodeFor->forLoop.generator = nodeGen;

  /* id <- <expr> */
  if (!GetIdentifier(p, &nodeGen->gen.name))
    goto failure;

  if (!Consume(p, TOKEN_LEFT_ARROW))
    goto failure;

  if (!(nodeGen->gen.from = ParseExpression(p)))
    goto failure;

  /* UNTIL <expr> or TO <expr> */
  switch (p->current.type) {
  case TOKEN_KEYWORD_TO:
    nodeGen->gen.IncludeMax = true;
    break;

  case TOKEN_KEYWORD_UNTIL:
    nodeGen->gen.IncludeMax = false;
    break;

  default:
    SyntaxError(p, "Invalid generator");
    goto failure;
  }
  if (!Advance(p))
    goto failure;

  /* optional: BY <expr> */
  if (!(p->current.type == TOKEN_KEYWORD_BY))
    goto failure;

  nodeFor->forLoop.body = ParseForBody(p);

failure:
  ASTFree(nodeGen);
  ASTFree(nodeFor);
  return NULL;
}

static ASTNode_t *ParseFor(Parser_t *p) {
  if (!Consume(p, TOKEN_KEYWORD_FOR))
    return NULL;

  return ParseLoop(p);
}

// <param> = id <type?>
static ASTNode_t *ParseParam(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_PARAM);
  if (node == NULL)
    return NULL;

  if (!GetIdentifier(p, &node->param.name))
    goto failure;

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
static bool ParseParamsC(Parser_t *p, ASTList_t *list) {
  while (p->current.type != TOKEN_RPAR) {
    if (!Consume(p, TOKEN_COMMA))
      return false;

    if (p->current.type == TOKEN_RPAR)
      return true;

    if (!AppendNode(p, list, ParseParam(p)))
      return false;
  }
  return true;
}
