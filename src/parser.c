/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "parser.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

/* helper functions */
static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind);
static bool AppendNode(Parser_t *p, ASTList_t *list, ASTNode_t *node);
static bool GetIdentifier(Parser_t *p, char **out);
static bool Consume(Parser_t *p, TokenType_t expected);
static bool ConsumeId(Parser_t *p, const char *name);
static bool ParserInit(Parser_t *p);
static ASTNode_t *BlockReturnExpr(Parser_t *p);
static ASTNode_t *WrapInReturn(Parser_t *p, ASTNode_t *node);

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
/* Extension */
static ASTNode_t *ParseLoop(Parser_t *p);
static ASTNode_t *ParseFor(Parser_t *p);
static ASTNode_t *ParseLoopBody(Parser_t *p);
static ASTNode_t *ParseIfExpr(Parser_t *p);
static ASTNode_t *ParseIfOrExpr(Parser_t *p);
static ASTNode_t *ParseBody(Parser_t *p);

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

  Parser_t p = {0};

  if (!ParserInit(&p)) {
    *errOut = p.errorCode;
    return NULL;
  }
  ASTNode_t *node = ParseRoot(&p);

  PPDelete(p.pp);

  *errOut = p.errorCode;

  if (node != NULL)
    return node;

  *errOut = (p.errorCode == ERROR_NOERR) ? ERROR_SYNTAX : p.errorCode;
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

/* on success returns node block which contains node return with value <expr>
 * else sets error and returns NULL
 */
static ASTNode_t *BlockReturnExpr(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_BLOCK);
  if (node == NULL)
    return NULL;

  ASTNode_t *ret = WrapInReturn(p, ParseIfOrExpr(p));
  if (ret == NULL)
    goto failure;

  if (!AppendNode(p, &node->block, ret))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// Wraps node in return node
// returns NULL and sets err, if unsuccessful
static ASTNode_t *WrapInReturn(Parser_t *p, ASTNode_t *node) {
  if (node == NULL)
    return NULL;

  ASTNode_t *ret = NewNode(p, NODE_RETURN);
  if (ret == NULL) {
    return NULL;
  }
  ret->pos = node->pos;
  ret->ret.value = node;
  return ret;
}

/* Error handling wrapper funtion */
static bool AppendNode(Parser_t *p, ASTList_t *list, ASTNode_t *node) {
  if (node == NULL)
    return false;

  if (!ASTListAppend(list, node)) {
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
  if (*out == NULL) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
    return false;
  }
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

// of failure, return false and set error
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
  if (!Consume(p, TOKEN_KW_IMPORT))
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
  case TOKEN_KW_INT:
    *out = TYPE_INT;
    break;

  case TOKEN_KW_DOUBLE:
    *out = TYPE_DOUBLE;
    break;

  case TOKEN_KW_BOOLEAN:
    *out = TYPE_BOOLEAN;
    break;

  case TOKEN_KW_UNIT:
    *out = TYPE_UNIT;
    break;

  case TOKEN_KW_STRING:
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

static bool SkipNewLines(Parser_t *p) {
  while (p->current.type == TOKEN_NEWLINE)
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
  return SkipNewLines(p);
}

// if LF:   <prog> = LF <prog>
// if EOF:  <prog> = {}
// if @:    <prog> = <main> <fndef> <prog>
// else:    <prog> = <fndef> <prog>
static bool ParseProg(Parser_t *p, ASTList_t *list) {
  while (true) {
    bool isMain = false;

    if (!SkipNewLines(p))
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
// else if last DEDENT:  <stmntsC> = <stmnts>
static bool ParseStatements(Parser_t *p, ASTList_t *list) {
  while (true) {
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
      /* If it was FOR, WHILE or IF which ended with block */
      if (p->last.type == TOKEN_DEDENT)
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

  if (!Consume(p, TOKEN_KW_RETURN))
    goto failure;

  switch (p->current.type) {
  case TOKEN_NEWLINE:
  case TOKEN_SEMICOLON:
  case TOKEN_EOF:
  case TOKEN_DEDENT:
  case TOKEN_KW_ELSE:
    break;

  default:
    if (!(node->ret.value = ParseIfOrExpr(p)))
      goto failure;
  }

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
  case TOKEN_KW_VAR:
  case TOKEN_KW_VAL:
    return ParseVarDef(p);

  case TOKEN_KW_IF:
    return ParseIfElse(p);

  case TOKEN_KW_WHILE:
    return ParseWhile(p);

  case TOKEN_KW_RETURN:
    return ParseReturn(p);

  case TOKEN_KW_FOR:
    return ParseFor(p);

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

  if (p->current.type == TOKEN_KW_VAL)
    kind = NODE_VALDEF;
  else if (p->current.type == TOKEN_KW_VAR)
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
    node->type = node->varDef.declType;
  } else {
    node->varDef.declType = TYPE_UNKNOWN;
    node->type = TYPE_UNKNOWN;
  }

  if (!Consume(p, TOKEN_EQ))
    goto failure;

  node->varDef.init = ParseIfOrExpr(p);

  if (node->varDef.init != NULL)
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

  if (!(node->assign.value = ParseIfOrExpr(p)))
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

  if (!Consume(p, TOKEN_KW_DEF))
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

  if (p->current.type == TOKEN_COLON) {
    if (!(ParseType(p, &node->fnDef.declType)))
      goto failure;
    node->type = node->fnDef.declType;
  } else {
    node->fnDef.declType = TYPE_UNKNOWN;
    node->type = TYPE_UNKNOWN;
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

// can be returned as value
static bool IsValueNode(ASTKind_t k) {
  switch (k) {
  case NODE_ID:
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
  case NODE_UNOP:
  case NODE_BINOP:
  case NODE_COND:
  case NODE_FNCALL:
  case NODE_IFEXPR:
    return true;
  default:
    return false;
  }
}

static bool AllBranchesReturn(Parser_t *p, ASTNode_t *block) {
  if (block == NULL)
    return false;

  ASTNode_t *last = block->block.items[block->block.count - 1];

  if (last->kind == NODE_RETURN)
    return true;

  if (last->kind == NODE_IF && last->ifElse.elseBlock != NULL) {
    return AllBranchesReturn(p, last->ifElse.thenBlock) &&
           AllBranchesReturn(p, last->ifElse.elseBlock);
  }
  ASTNode_t *ret = NULL;
  if (IsValueNode(last->kind)) {
    if (!(ret = WrapInReturn(p, last)))
      return false;
    block->block.items[block->block.count - 1] = ret;

  } else {
    if (!(ret = WrapInReturn(p, NewNode(p, NODE_UNIT))))
      return false;

    if (!AppendNode(p, &block->block, ret))
      return false;

    ret->ret.value->type = TYPE_UNIT;
  }

  return true;
}

// if LF: <fnbody> = <block>
// else: <fnbody> = block(<expr>) LF  /* one-line body */
static ASTNode_t *ParseFnBody(Parser_t *p) {
  ASTNode_t *node = NULL;
  if (p->current.type == TOKEN_NEWLINE) {
    node = ParseBlock(p);

    if (node == NULL)
      return NULL;

    if (node->block.count <= 0) {
      ReportInternalError("Empty function body");
      p->errorCode = ERROR_INTERNAL;
      goto failure;
    }

    if (!AllBranchesReturn(p, node))
      goto failure;

    return node;
  }

  /* one-line function body */
  node = BlockReturnExpr(p);
  if (node == NULL)
    return NULL;

  if (p->current.type == TOKEN_EOF || p->last.type == TOKEN_DEDENT)
    return node;

  if (!Consume(p, TOKEN_NEWLINE)) {
    goto failure;
  }
  return node;

failure:
  ASTFree(node);
  return NULL;
}

// <ifelse> = if <expr> then <block> <else?>
// <else?> = <else>
// <else?> = epsilon
// <else> = <ifelse>
// <else> = <block>
static ASTNode_t *ParseIfElse(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_IF);
  if (node == NULL)
    return NULL;

  if (!Consume(p, TOKEN_KW_IF))
    goto failure;

  if (!(node->ifElse.cond = ParseExpression(p)))
    goto failure;

  if (!Consume(p, TOKEN_KW_THEN))
    goto failure;

  if (!(node->ifElse.thenBlock = ParseBody(p)))
    goto failure;

  if (p->current.type == TOKEN_NEWLINE && p->next.type == TOKEN_KW_ELSE)
    if (!Advance(p))
      goto failure;

  if (p->current.type == TOKEN_KW_ELSE) {
    if (!Consume(p, TOKEN_KW_ELSE))
      goto failure;

    if (p->current.type == TOKEN_KW_IF) {
      ASTNode_t *block = NewNode(p, NODE_BLOCK);
      if (block == NULL)
        goto failure;

      node->ifElse.elseBlock = block;
      if (!AppendNode(p, &block->block, ParseIfElse(p)))
        goto failure;

    } else {
      if (!(node->ifElse.elseBlock = ParseBody(p)))
        goto failure;
    }
  }
  return node;

failure:
  ASTFree(node);
  return NULL;
}

// all statements except vardef
static ASTNode_t *ParseBody(Parser_t *p) {
  if (p->current.type == TOKEN_NEWLINE)
    return ParseBlock(p);

  if (p->current.type == TOKEN_KW_VAL || p->current.type == TOKEN_KW_VAR) {
    SyntaxError(p, "Invalid function body");
    return NULL;
  }
  ASTNode_t *block = NewNode(p, NODE_BLOCK);
  if (block == NULL)
    return NULL;

  if (!(AppendNode(p, &block->block, ParseStatement(p)))) {
    ASTFree(block);
    return NULL;
  }

  return block;
}

// <while> = while <expr> do <block>
static ASTNode_t *ParseWhile(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_WHILE);
  if (node == NULL)
    return NULL;

  if (!Consume(p, TOKEN_KW_WHILE))
    goto failure;

  if (!(node->whileDo.cond = ParseExpression(p)))
    goto failure;

  if (!Consume(p, TOKEN_KW_DO))
    goto failure;

  if (!(node->whileDo.body = ParseBody(p)))
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

  if (!(ParseType(p, &node->param.declType)))
    goto failure;

  node->type = node->param.declType;

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

// <iforexpr> = <expr>
// <iforexpr> = <IfExpr>
static ASTNode_t *ParseIfOrExpr(Parser_t *p) {
  if (p->current.type == TOKEN_KW_IF)
    return ParseIfExpr(p);
  else
    return ParseExpression(p);
}

// <Exprbranch> = <block>
// <exprbranch> = <iforexpr>
static ASTNode_t *ParseExprBranch(Parser_t *p) {
  if (p->current.type == TOKEN_NEWLINE)
    return ParseBlock(p);
  return ParseIfOrExpr(p);
}

// <IfExpr> = if <cond> then <ifOrExpr> else <ifOrExpr>
static ASTNode_t *ParseIfExpr(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_IFEXPR);
  if (node == NULL)
    return NULL;

  if (!Consume(p, TOKEN_KW_IF))
    goto failure;

  if (!(node->ifexpr.cond = ParseExpression(p)))
    goto failure;

  if (!Consume(p, TOKEN_KW_THEN))
    goto failure;

  if (!(node->ifexpr.thenExpr = ParseExprBranch(p)))
    goto failure;

  if (p->current.type == TOKEN_NEWLINE && p->next.type == TOKEN_KW_ELSE)
    if (!Advance(p))
      goto failure;

  if (!Consume(p, TOKEN_KW_ELSE))
    goto failure;

  if (!(node->ifexpr.elseExpr = ParseExprBranch(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// <loopBody> = LF if <cond> <loopBody>
// <loopBody> = LF DEDENT DO <block>
static ASTNode_t *ParseLoopBody(Parser_t *p) {
  ASTNode_t *root = NewNode(p, NODE_BLOCK);
  if (root == NULL)
    return NULL;
  ASTNode_t *nested = root;
  ASTNode_t **nestedPtr = &root;

  while (true) {
    if (p->current.type == TOKEN_NEWLINE)
      if (!Advance(p))
        goto failure;

    switch (p->current.type) {

    case TOKEN_KW_IF: {
      if (!Consume(p, TOKEN_KW_IF))
        goto failure;
      /* Create new node IF */
      ASTNode_t *node = NewNode(p, NODE_IF);
      if (node == NULL)
        goto failure;

      if (!AppendNode(p, &nested->block, node))
        goto failure;

      if (!(node->ifElse.cond = ParseExpression(p)))
        goto failure;

      if (!(node->ifElse.thenBlock = NewNode(p, NODE_BLOCK)))
        goto failure;

      nested = node->ifElse.thenBlock;
      nestedPtr = &node->ifElse.thenBlock;
      break;
    }

    case TOKEN_DEDENT: {
      if (!Consume(p, TOKEN_DEDENT))
        goto failure;

      if (!Consume(p, TOKEN_KW_DO))
        goto failure;

      ASTNode_t *node = ParseBody(p);
      if (node == NULL)
        goto failure;

      ASTFree(*nestedPtr);

      *nestedPtr = node;

      return root;
    }
    default:
      SyntaxError(p, "Invalid for loop");
      goto failure;
    }
  }
failure:
  ASTFree(root);
  return NULL;
}

// <gen> = id <- <expr> <genEnd>
// <genEnd> = to    <expr> [<step>]
// <genEnd> = until <expr> [<step>]
static ASTNode_t *ParseGen(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_GEN);
  if (node == NULL)
    return NULL;

  if (!GetIdentifier(p, &node->gen.name))
    goto failure;

  if (!Consume(p, TOKEN_LEFT_ARROW))
    goto failure;

  if (!(node->gen.from = ParseExpression(p)))
    goto failure;

  /* UNTIL <expr> or TO <expr> */
  switch (p->current.type) {
  case TOKEN_KW_TO:
    node->gen.IncludeMax = true;
    break;

  case TOKEN_KW_UNTIL:
    node->gen.IncludeMax = false;
    break;

  default:
    SyntaxError(p, "Invalid generator");
    goto failure;
  }
  if (!Advance(p))
    goto failure;

  if (!(node->gen.to = ParseExpression(p)))
    goto failure;

  /* optional: BY <expr> */
  if (p->current.type == TOKEN_KW_BY) {
    if (!Consume(p, TOKEN_KW_BY))
      goto failure;
    if (!(node->gen.by = ParseExpression(p)))
      goto failure;
  }
  return node;

failure:
  ASTFree(node);
  return NULL;
}

// <loop> = <gen> DO <block>
// <loop> = LF INDENT <gen> <loopBody>
static ASTNode_t *ParseLoop(Parser_t *p) {
  ASTNode_t *node = NewNode(p, NODE_FOR);
  if (node == NULL)
    return NULL;

  /* for LF */
  if (p->current.type == TOKEN_NEWLINE) {

    if (!Consume(p, TOKEN_NEWLINE))
      goto failure;

    if (!Consume(p, TOKEN_INDENT))
      goto failure;

    if (!(node->forLoop.gen = ParseGen(p)))
      goto failure;

    if (!(node->forLoop.body = ParseLoopBody(p)))
      goto failure;

    return node;
  }
  /* for <gen> */
  if (!(node->forLoop.gen = ParseGen(p)))
    goto failure;

  if (!Consume(p, TOKEN_KW_DO))
    goto failure;

  if (!(node->forLoop.body = ParseBody(p)))
    goto failure;

  return node;

failure:
  ASTFree(node);
  return NULL;
}

// <for> = for <loop>
static ASTNode_t *ParseFor(Parser_t *p) {
  if (!Consume(p, TOKEN_KW_FOR))
    return NULL;

  return ParseLoop(p);
}