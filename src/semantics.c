/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "semantics.h"

#include "bin_tree.h"
#include "int_stack.h"
#include "messenger.h"
#include <assert.h>
#include <string.h>

#define A_PARAM_BLOCK 0

/*
 * Helper functions
 */

static inline MsgContext_t SpanToCtxUnderline(SourceSpan_t span) {
  return MSG_CONTEXT_T(.lineNum = span.line, .hlStartCol = span.start,
                       .hlEndCol = span.end, .poiCol = 0);
}

static inline const char *GetIDStr(const char *id) {
  return id ? id : "<unknown>";
}

/*
 * Symbol definitions
 */

typedef enum {
  VS_UNDEFINED = 0,
  VS_MUTABLE,
  VS_CONST,
} VarState_t;

typedef struct {
  int blockID;
  DataType_t dtype;
  VarState_t vstate;

  /* Debug info */
  SourceSpan_t pos;
  const char *id;
} VarSymbol_t;

static VarSymbol_t *CreateVarSymbol(void) {
  VarSymbol_t *s;
  s = malloc(sizeof *s);
  if (!s)
    return NULL;

  s->blockID = -1;
  s->dtype = TYPE_UNKNOWN;
  s->vstate = VS_UNDEFINED;

  s->pos = (SourceSpan_t){.line = 0, .start = 0, .end = 0};
  s->id = NULL;

  return s;
}

static void DeleteVarSymbol(void *data) {
  VarSymbol_t *s = data;
  if (!s)
    return;

  free((void *)s->id);
  free(s);
}

typedef struct {
  SymbolTree_t symbols;
  DataType_t returnType;
  bool isMain;

  /* Debug info */
  SourceSpan_t pos;
  const char *id;
} FunSymbol_t;

static FunSymbol_t *CreateFunSymbol(void) {
  FunSymbol_t *s;
  s = malloc(sizeof *s);
  if (!s)
    return NULL;

  s->id = NULL;
  s->pos = (SourceSpan_t){.line = 0, .start = 0, .end = 0};
  s->isMain = false;
  s->returnType = TYPE_UNKNOWN;
  if (!SymbolTreeInitWithDestructor(&s->symbols, DeleteVarSymbol)) {
    free(s);
    return NULL;
  }

  return s;
}

static void DeleteFunSymbol(void *data) {
  FunSymbol_t *s = data;
  if (!s)
    return;

  free((void *)s->id);
  SymbolTreeDestroy(&s->symbols);
  free(s);
}

/*
 * Analyzer definitions
 */

struct analyzer {
  SymbolTree_t funs;
  FunSymbol_t *thisFun;

  struct {
    DataType_t type;
    int blockID;
  } lastType;

  IntStack_t *blockStack;
  int blockID;
};

Analyzer_t *CreateAnalyzer(void) {
  Analyzer_t *a;
  a = malloc(sizeof *a);
  if (!a)
    return NULL;

  a->thisFun = NULL;
  a->lastType.type = TYPE_UNKNOWN;
  a->lastType.blockID = 0;
  if (!SymbolTreeInitWithDestructor(&a->funs, DeleteFunSymbol)) {
    free(a);
    return NULL;
  }

  a->blockID = 0;
  a->blockStack = IntStackCreate();
  if (!a->blockStack) {
    free(a);
    return NULL;
  }

  return a;
}

void DeleteAnalyzer(Analyzer_t *a) {
  if (!a)
    return;

  SymbolTreeDestroy(&a->funs);
  IntStackTerminate(a->blockStack);
  free(a);
}

AError_t A_StartFunction(Analyzer_t *a, const ASTNode_t *fndef) {
  assert(a && fndef && fndef->kind == NODE_FNDEF);

  a->thisFun = NULL;
  AError_t err = AERR_OK;

  /* dedent all */
  while (!IntStackIsEmpty(a->blockStack))
    A_EndBlock(a);
  a->blockID = A_PARAM_BLOCK;

  /* Creaate function symbol */
  FunSymbol_t *fs = CreateFunSymbol();
  if (!fs)
    return AERR_MEM;

  fs->returnType = fndef->fnDef.declType;
  fs->isMain = fndef->fnDef.isMain;
  fs->pos = fndef->pos;

  size_t idLen = strlen(fndef->fnDef.name);
  fs->id = malloc(idLen);
  if (!fs->id)
    return AERR_MEM;
  memcpy((void *)fs->id, fndef->fnDef.name, idLen);

  /* Add parameters */
  AST_LIST_FOREACH(fndef->fnDef.params, param) {
    VarSymbol_t *vs = CreateVarSymbol();
    if (!vs) {
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    vs->dtype = param->param.declType;
    vs->blockID = a->blockID;

    /* check if the parameter has a type (sanity check) */
    assert(vs->dtype != TYPE_UNKNOWN && vs->dtype != TYPE_NA);

    if (!SymbolTreeInsertWithData(&fs->symbols, param->param.name, vs)) {
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }
  }

  /* redefinition error */
  const SymbolHeader_t *symbol = SymbolTreeSearch(&a->funs, fndef->fnDef.name);
  if (symbol) {
    err = AERR_SEM;

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_REDEF, "Function `%s' already exists.",
                 fndef->fnDef.name);
    MsgAddContext(msg, SpanToCtxUnderline(((FunSymbol_t *)symbol->data)->pos),
                  "First defined here.");
    MsgSend(msg);

    /* add anyways */
  }

  /* store the function symbol */
  if (!SymbolTreeInsertWithData(&a->funs, fndef->fnDef.name, fs)) {
    DeleteFunSymbol(fs);
    return AERR_MEM;
  }

  a->thisFun = fs;
  return err;
}

AError_t A_BeginBlock(Analyzer_t *a) {
  assert(a);

  return IntStackPush(a->blockStack, a->blockID++) ? AERR_OK : AERR_MEM;
}

AError_t A_EndBlock(Analyzer_t *a) {
  assert(a);

  AError_t err = AERR_OK;

  IntStackPop(a->blockStack);
  if (!IntStackIsEmpty(a->blockStack))
    return AERR_OK;

  /* statement variable return thing */
  DataType_t rtype = TYPE_UNIT;
  if (a->lastType.blockID == A_PARAM_BLOCK + 1)
    rtype = a->lastType.type;

  /* assign type if it isn't */
  if (a->thisFun->returnType == TYPE_UNKNOWN)
    a->thisFun->returnType = rtype;

  if (a->thisFun->returnType != rtype) {
    /* error: incompatible types */

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "Incompatible return type `%s' != `%s' at "
                              "the end of function `%s'.",
                              DataTypeToStr(a->thisFun->returnType),
                              DataTypeToStr(rtype), GetIDStr(a->thisFun->id));
    MsgAddContext(msg, SpanToCtxUnderline(a->thisFun->pos), "here");
    MsgSend(msg);

    err = AERR_SEM;
  }

  a->thisFun = NULL;
  return err;
}

/* returns NULL if not found (or error) */
static VarSymbol_t *GetActiveVSymbol(Analyzer_t *a, const char *id) {
  assert(a && id);

  long vsDepth = -1;
  VarSymbol_t *vs = NULL;

  SymbolTreeIndexNode_t *symbol;
  SymbolTreeFindAllIndices(&a->thisFun->symbols, id, &symbol);
  for (; symbol; symbol = symbol->next) {
    const SymbolHeader_t *sh =
        SymbolTreeAt(&a->thisFun->symbols, symbol->index);
    long d = IntStackDepth(a->blockStack, ((VarSymbol_t *)sh->data)->blockID);

    if (vsDepth < 0 || (d < vsDepth && d >= 0)) {
      vsDepth = d;
      vs = (VarSymbol_t *)sh->data;
    }
  }

  return vs;
}

static AError_t AnalyzeOp(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root);
  /* TODO: implement */
  return AERR_OK;
}

static AError_t AnalyzeFnCall(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root);
  /* TODO: implement */
  return AERR_OK;
}

/* sets last type */
static AError_t AnalyzeExpr(Analyzer_t *a, ASTNode_t *root) {
  /* TODO:
   * def foo() = 4
   */
  assert(a && root);
  /* TODO: implement */
  return AERR_OK;
}

static AError_t AnalyzeAssign(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_ASSIGN);

  AError_t err = AERR_OK;

  VarSymbol_t *vs = GetActiveVSymbol(a, root->assign.name);
  if (!vs) {
    /* create the symbol if it doesn't exist */
    vs = CreateVarSymbol();
    if (!vs)
      return AERR_MEM;

    vs->blockID = a->blockID;
    vs->dtype = root->varDef.declType;
    vs->vstate = VS_UNDEFINED;
    vs->pos = root->pos;

    size_t idLen = strlen(root->varDef.name) + 1;
    vs->id = malloc(idLen);
    if (!vs->id)
      return AERR_MEM;
    memcpy((void *)vs->id, root->varDef.name, idLen);
  }

  if (vs->vstate == VS_UNDEFINED) {
    /* undefined variable error */
    err = AERR_SEM;

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                 "Variable `%s' is assigned to but it's not defined.",
                 root->assign.name);
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "here");
    MsgSend(msg);
  }

  /* expr evaluation & type check */
  AError_t e = AnalyzeExpr(a, root->assign.value);
  err = err != AERR_OK ? err : e;

  if (vs->dtype == TYPE_UNKNOWN)
    vs->dtype = a->lastType.type;

  if (vs->dtype != a->lastType.type) {
    /* incorrect type error */
    err = AERR_SEM;

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                 "Incompatible type at assignment: typeof `%s' (`%s') != `%s'",
                 GetIDStr(vs->id), DataTypeToStr(vs->dtype),
                 DataTypeToStr(a->lastType.type));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "here");
    MsgSend(msg);
  }

  /* keep the last type goin' */
  a->lastType.type = vs->dtype;
  a->lastType.blockID = a->blockID;

  return err;
}

static AError_t AnalyzeVarDef(Analyzer_t *a, ASTNode_t *root, bool isMutable) {
  assert(a && root && root->kind == NODE_VARDEF);

  AError_t err = AERR_OK;

  VarSymbol_t *vs = GetActiveVSymbol(a, root->varDef.name);

  if (vs && vs->blockID == a->blockID && vs->vstate == VS_MUTABLE) {
    /* redefinition error */
    err = AERR_SEM;

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_REDEF, "Variable `%s' already exists.",
                 root->varDef.name);
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "First defined here.");
    MsgSend(msg);

    /* process like it is assignment */
  }

  /* create if it doesn't exist*/
  if (vs == NULL) {
    vs = CreateVarSymbol();
    if (!vs)
      return AERR_MEM;

    vs->blockID = a->blockID;
    vs->dtype = root->varDef.declType;
    vs->vstate = isMutable ? VS_MUTABLE : VS_CONST;
    vs->pos = root->pos;

    size_t idLen = strlen(root->varDef.name) + 1;
    vs->id = malloc(idLen);
    if (!vs->id)
      return AERR_MEM;
    memcpy((void *)vs->id, root->varDef.name, idLen);
  }

  /* keep track of last type */
  a->lastType.type = vs->dtype;
  a->lastType.blockID = a->blockID;

  /* treat as assignment */
  ASTNode_t assign = *root;
  assign.kind = NODE_ASSIGN;
  assign.assign.value = root->varDef.init;

  bool anotheroneoftheseerrors = AnalyzeAssign(a, &assign);
  return err != AERR_OK ? err : anotheroneoftheseerrors;
}

static AError_t AnalyzeIf(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_IF);
  /* TODO: implement */
  return AERR_OK;
}

static AError_t AnalyzeWhile(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_WHILE);
  /* TODO: implement */
  return AERR_OK;
}

static AError_t AnalyzeReturn(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_RETURN);
  /* TODO: implement */
  return AERR_OK;
}

AError_t A_AnalyzeStatement(Analyzer_t *a, ASTNode_t *stmnt) {
  assert(a && stmnt);

  switch (stmnt->kind) {
  case NODE_ASSIGN:
    return AnalyzeAssign(a, stmnt);

  case NODE_VARDEF:
    return AnalyzeVarDef(a, stmnt, true);

  case NODE_VALDEF:
    return AnalyzeVarDef(a, stmnt, false);

  case NODE_IF:
    return AnalyzeIf(a, stmnt);

  case NODE_WHILE:
    return AnalyzeWhile(a, stmnt);

  case NODE_RETURN:
    return AnalyzeReturn(a, stmnt);

  case NODE_UNOP:
  case NODE_BINOP:
  case NODE_FNCALL:
  case NODE_ID:
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
    return AnalyzeExpr(a, stmnt);

  default:
    break;
  }

  return AERR_OK;
}
