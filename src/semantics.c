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
#define A_MAX_BUILTINS_PARAMS_COUNT 4

/* Builtin function `declarations'
 *   DO(name, returnType, paramCount, params...)
 *   P(name, type) // param
 *                 // if there are no params P("", TYPE_NA) is set
 */
#define A_FOR_BUILTINS(DO, P)                                                  \
  DO("print", TYPE_UNIT, 1, P("value", TYPE_ANY))                              \
  DO("readLine", TYPE_STRING, 0, P("", TYPE_NA))                               \
  DO("readInt", TYPE_INT, 0, P("", TYPE_NA))                                   \
  DO("readDouble", TYPE_DOUBLE, 0, P("", TYPE_NA))                             \
  DO("toStr", TYPE_STRING, 1, P("value", TYPE_ANY))                            \
  DO("toInt", TYPE_INT, 1, P("value", TYPE_DOUBLE))                            \
  DO("strLength", TYPE_INT, 1, P("value", TYPE_STRING))                        \
  DO("subStr", TYPE_STRING, 3, P("value", TYPE_STRING), P("start", TYPE_INT),  \
     P("end", TYPE_INT))                                                       \
  DO("ord", TYPE_INT, 2, P("value", TYPE_STRING), P("index", TYPE_INT))        \
  DO("chr", TYPE_STRING, 1, P("code", TYPE_INT))

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

static inline const char *CopyStr(const char *s) {
  size_t len = strlen(s) + 1;
  char *new = malloc(len);
  if (!new)
    return NULL;
  memcpy((void *)new, s, len);
  return new;
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
  bool isDefined;

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
  s->isDefined = false;
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

  IntStack_t *blockStack;
  int blockID;
};

static AError_t PopulateBuiltins(Analyzer_t *a);

Analyzer_t *CreateAnalyzer(void) {
  Analyzer_t *a;
  a = malloc(sizeof *a);
  if (!a)
    return NULL;

  a->thisFun = NULL;
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

  /* define the builtins */
  if (AERR_MEM == PopulateBuiltins(a)) {
    DeleteAnalyzer(a);
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
  fs->isDefined = true;
  fs->pos = fndef->pos;

  fs->id = CopyStr(fndef->fnDef.name);
  if (!fs->id) {
    DeleteFunSymbol(fs);
    return AERR_MEM;
  }

  /* Add parameters */
  AST_LIST_FOREACH(fndef->fnDef.params, param) {
    VarSymbol_t *vs = CreateVarSymbol();
    if (!vs) {
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    vs->dtype = param->param.declType;
    vs->blockID = A_PARAM_BLOCK;

    vs->id = CopyStr(param->param.name);
    if (!vs->id) {
      DeleteVarSymbol(vs);
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    /* check if the parameter has a type (sanity check) */
    assert(vs->dtype != TYPE_UNKNOWN && vs->dtype != TYPE_NA);

    if (!SymbolTreeInsertWithData(&fs->symbols, vs->id, vs)) {
      DeleteVarSymbol(vs);
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
  if (!SymbolTreeInsertWithData(&a->funs, fs->id, fs)) {
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

  IntStackPop(a->blockStack);
  if (IntStackIsEmpty(a->blockStack))
    a->thisFun = NULL;

  return AERR_OK;
}

/*
 * Expression analysis
 */

static AError_t AnalyzeBinOp(Analyzer_t *a, ASTNode_t *root, DataType_t *type);
static AError_t AnalyzeUnaryOp(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type);
static AError_t AnalyzeFnCall(Analyzer_t *a, ASTNode_t *root, DataType_t *type);
static AError_t AnalyzeLiteral(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type);
static AError_t AnalyzeExpr(Analyzer_t *a, ASTNode_t *root, DataType_t *type);

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

static void ReportBinOpValueError(const char *op, DataType_t ltype,
                                  DataType_t rtype, SourceSpan_t opPos,
                                  SourceSpan_t lpos, SourceSpan_t rpos) {
  Message_t *msg =
      MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
               "Unsupported operation `%s' for types `%s' and `%s'", op,
               DataTypeToStr(ltype), DataTypeToStr(rtype));
  MsgAddContext(msg, SpanToCtxUnderline(opPos), "used here");
  MsgAddContext(msg, SpanToCtxUnderline(lpos), "typeof left expression: `%s'",
                ltype);
  MsgAddContext(msg, SpanToCtxUnderline(rpos), "typeof right expression: `%s'",
                rtype);
  MsgSend(msg);
}

static AError_t CorrectMathTypes(ASTNode_t **left, ASTNode_t **right,
                                 DataType_t ltype, DataType_t rtype,
                                 DataType_t *result) {
  *result = ltype;
  if (ltype == rtype)
    return AERR_OK;
  *result = TYPE_DOUBLE;

  ASTNode_t *conv = ASTCreate(NODE_CONV_DOUBLE, (SourceSpan_t){0});
  if (!conv)
    return AERR_MEM;

  if (ltype == TYPE_INT) {
    conv->value = *left;
    *left = conv;
  } else {
    conv->value = *right;
    *right = conv;
  }

  return AERR_OK;
}

/* if `type' is TYPE_UNKNOWN it is set to the expression type
 * if `type' is not equal to the expression type => throw error and return
 *   the origitnal type
 */
static AError_t AnalyzeBinOp(Analyzer_t *a, ASTNode_t *root, DataType_t *type) {
  assert(a && root && root->kind == NODE_BINOP);

  AError_t err = AERR_OK;
  DataType_t otype = TYPE_UNKNOWN;

  /* get subexpression types */
  DataType_t ltype = TYPE_UNKNOWN;
  DataType_t rtype = TYPE_UNKNOWN;
  err = AnalyzeExpr(a, root->binOp.left, &ltype);
  AError_t e = AnalyzeExpr(a, root->binOp.right, &rtype);
  err = err != AERR_OK ? err : e;

  if (err == AERR_MEM)
    return err;

  SourceSpan_t lpos = root->binOp.left->pos;
  SourceSpan_t rpos = root->binOp.right->pos;

  switch (root->binOp.op) {
  /* maths */
  case OP_DIV:
  case OP_MUL:
  case OP_MINUS: {
    if ((ltype != TYPE_INT && ltype != TYPE_DOUBLE) ||
        (rtype != TYPE_INT && rtype != TYPE_DOUBLE)) {
      /* type error */
      err = AERR_SEM;
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                     &otype);
  } break;

  /* special */
  case OP_PLUS: {
    if ((ltype != TYPE_INT && ltype != TYPE_DOUBLE) ||
        (rtype != TYPE_INT && rtype != TYPE_DOUBLE) ||
        (ltype != TYPE_STRING || rtype != TYPE_STRING)) {
      /* type error */
      err = AERR_SEM;
      ReportBinOpValueError("+", ltype, rtype, root->pos, lpos, rpos);
      break;
    }

    if (ltype != TYPE_STRING)
      CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                       &otype);
    else
      otype = TYPE_STRING;
  } break;

  /* relations */
  case OP_LTE:
  case OP_LT:
  case OP_GTE:
  case OP_GT: {
    if ((ltype != TYPE_INT && ltype != TYPE_DOUBLE) ||
        (rtype != TYPE_INT && rtype != TYPE_DOUBLE) ||
        (ltype != TYPE_STRING || rtype != TYPE_STRING)) {
      /* type error */
      err = AERR_SEM;
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    if (ltype != TYPE_STRING)
      CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                       &otype);

    otype = TYPE_BOOLEAN;
  } break;

  case OP_EQ:
  case OP_NEQ: {
    if ((ltype != TYPE_INT && ltype != TYPE_DOUBLE) ||
        (rtype != TYPE_INT && rtype != TYPE_DOUBLE) || (ltype != rtype)) {
      /* type error */
      err = AERR_SEM;
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    if (ltype != rtype)
      CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                       &otype);

    otype = TYPE_BOOLEAN;
  } break;

  /* default */
  default:
    break;
  }

  if (*type == TYPE_UNKNOWN)
    *type = otype;

  if (*type != otype) {
    /* type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "Unexpected type: expected `%s' got `%s'.",
                              DataTypeToStr(*type), DataTypeToStr(otype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  return err;
}

/* if `type' is TYPE_UNKNOWN it is set to the expression type
 * if `type' is not equal to the expression type => throw error and return
 *   the origitnal type
 */
static AError_t AnalyzeUnaryOp(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type) {
  assert(a && root && root->kind == NODE_UNOP);

  AError_t err = AERR_OK;

  DataType_t otype = TYPE_UNKNOWN;

  switch (root->unOp.op) {
  case OP_UMINUS:
  case OP_UPLUS: {
    err = AnalyzeExpr(a, root->unOp.operand, &otype);

    if (otype != TYPE_INT && otype != TYPE_DOUBLE) {
      /* type error */
      err = AERR_SEM;

      Message_t *msg =
          MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                   "Type error: expected `%s' or `%s' != got `%s'",
                   DataTypeToStr(TYPE_INT), DataTypeToStr(TYPE_DOUBLE),
                   DataTypeToStr(otype));
      MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
      MsgAddContext(msg, SpanToCtxUnderline(root->unOp.operand->pos),
                    "typeof expression is `%s'", DataTypeToStr(otype));
      MsgSend(msg);
    }
  } break;
  default:
    break;
  }

  if (*type == TYPE_UNKNOWN)
    *type = otype;

  if (*type != otype) {
    /* type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "Unexpected type: expected `%s' got `%s'.",
                              DataTypeToStr(*type), DataTypeToStr(otype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  return err;
}

/* if `type' is TYPE_UNKNOWN it is set to the expression type
 * if `type' is not equal to the expression type => throw error and return
 *   the origitnal type
 */
static AError_t AnalyzeFnCall(Analyzer_t *a, ASTNode_t *root,
                              DataType_t *type) {
  assert(a && root && root->kind == NODE_FNCALL);
  /* TODO: implement */
  return AERR_OK;
}

static void MsgAddLiteralCtx(Message_t *msg, ASTNode_t *lit) {
  switch (lit->kind) {
  case NODE_INT:
    MsgAddContext(msg, SpanToCtxUnderline(lit->pos), "literal: `%d'",
                  lit->intVal);
    break;
  case NODE_DOUBLE:
    MsgAddContext(msg, SpanToCtxUnderline(lit->pos), "literal: `%lf'",
                  lit->dblVal);
    break;
  case NODE_STRING:
    MsgAddContext(msg, SpanToCtxUnderline(lit->pos), "literal: `%s'",
                  lit->string);
    break;
  case NODE_UNIT:
    MsgAddContext(msg, SpanToCtxUnderline(lit->pos), "literal: `()'");
    break;
  default:
    MsgAddContext(msg, SpanToCtxUnderline(lit->pos), "literal: `<unknown>'");
    break;
  }
}

/* if `type' is TYPE_UNKNOWN it is set to the expression type
 * if `type' is not equal to the expression type => throw error and return
 *   the origitnal type
 */
static AError_t AnalyzeLiteral(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type) {
  assert(a && root);

  AError_t err = AERR_OK;

  /* get the literal's type */
  DataType_t ltype;
  switch (root->kind) {
  case NODE_INT:
    ltype = TYPE_INT;
    break;
  case NODE_DOUBLE:
    ltype = TYPE_DOUBLE;
    break;
  case NODE_STRING:
    ltype = TYPE_STRING;
    break;
  case NODE_UNIT:
    ltype = TYPE_UNIT;
    break;
  default:
    ltype = TYPE_UNKNOWN;
  }

  if (*type == TYPE_UNKNOWN)
    *type = ltype;

  if (*type != ltype) {
    /* incompatible types */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "Unexpected type of literal `%s' != `%s'",
                              DataTypeToStr(ltype), DataTypeToStr(*type));
    MsgAddLiteralCtx(msg, root);
    MsgSend(msg);
  }

  return err;
}

/* if `type' is TYPE_UNKNOWN it is set to the expression type
 * if `type' is not equal to the expression type => throw error and return
 *   the origitnal type
 */
static AError_t AnalyzeExpr(Analyzer_t *a, ASTNode_t *root, DataType_t *type) {
  assert(a && root);

  AError_t err = AERR_OK;

  switch (root->kind) {
  case NODE_UNOP:
    return AnalyzeUnaryOp(a, root, type);
  case NODE_BINOP:
    return AnalyzeBinOp(a, root, type);
  case NODE_FNCALL:
    return AnalyzeFnCall(a, root, type);
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
    return AnalyzeLiteral(a, root, type);
  case NODE_ID:
    break; /* analyzed below */
  default:
    return err;
  }

  /* Analyze ID */

  VarSymbol_t *vs = GetActiveVSymbol(a, root->name);
  if (!vs) {
    /* create the symbol if it doesn't exist */
    vs = CreateVarSymbol();
    if (!vs)
      return AERR_MEM;

    vs->blockID = a->blockID;
    vs->dtype = TYPE_UNKNOWN;
    vs->vstate = VS_UNDEFINED;
    vs->pos = root->pos;

    vs->id = CopyStr(root->varDef.name);
    if (!vs->id) {
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }
  }

  if (vs->vstate == VS_UNDEFINED) {
    /* undefined variable error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                              "Variable `%s' is used but not defined.", vs->id);
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first appeared here");
    MsgSend(msg);
  }

  if (*type == TYPE_UNKNOWN)
    *type = vs->dtype;

  if (*type != vs->dtype) {
    /* type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(
        MSG_ERROR, ERROR_SEM_TYPE,
        "Variable `%s' is of uncompatible type. Expected `%s' got `%s'.",
        vs->id, DataTypeToStr(*type), DataTypeToStr(vs->dtype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  return err;
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

    vs->id = CopyStr(root->varDef.name);
    if (!vs->id) {
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }
  }

  if (vs->vstate == VS_UNDEFINED) {
    /* undefined variable error */
    err = AERR_SEM;

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                 "Variable `%s' is assigned to but it's not defined.",
                 root->assign.name);
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "assigned here");
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first appeared here");
    MsgSend(msg);
  }

  /* expr evaluation & type check */
  DataType_t type = TYPE_UNKNOWN;
  AError_t e = AnalyzeExpr(a, root->assign.value, &type);
  err = err != AERR_OK ? err : e;

  if (vs->dtype == TYPE_UNKNOWN)
    vs->dtype = type;

  if (vs->dtype != type) {
    /* incorrect type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(
        MSG_ERROR, ERROR_SEM_TYPE,
        "Incompatible type at assignment: typeof `%s' (`%s') != `%s'",
        GetIDStr(vs->id), DataTypeToStr(vs->dtype), DataTypeToStr(type));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "here");
    MsgSend(msg);
  }

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

    vs->id = CopyStr(root->varDef.name);
    if (!vs->id) {
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }
  }

  /* treat as assignment */
  ASTNode_t assign = *root;
  assign.kind = NODE_ASSIGN;
  assign.assign.value = root->varDef.init;

  bool anotheroneoftheseerrors = AnalyzeAssign(a, &assign);
  return err != AERR_OK ? err : anotheroneoftheseerrors;
}

static AError_t AnalyzeIf(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_IF);

  /* The if-else blocks are handled elswhere by calling `A_'-ops */

  DataType_t type = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->ifElse.cond, &type);

  if (type != TYPE_BOOLEAN) {
    /* incorrect type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "If statement requires `%s' got `%s'.",
                              DataTypeToStr(TYPE_BOOLEAN), DataTypeToStr(type));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "at if statement");
    MsgAddContext(msg, SpanToCtxUnderline(root->ifElse.cond->pos),
                  "condition is of type `%s'", DataTypeToStr(type));
    MsgSend(msg);
  }

  return err;
}

static AError_t AnalyzeWhile(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_WHILE);

  /* The while body is handled elswhere by calling `A_'-ops */

  DataType_t type = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->whileDo.cond, &type);

  if (type != TYPE_BOOLEAN) {
    /* incorrect type error */
    err = AERR_SEM;

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "While statement requires `%s' got `%s'.",
                              DataTypeToStr(TYPE_BOOLEAN), DataTypeToStr(type));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "at while statement");
    MsgAddContext(msg, SpanToCtxUnderline(root->whileDo.cond->pos),
                  "condition is of type `%s'", DataTypeToStr(type));
    MsgSend(msg);
  }

  return err;
}

static AError_t AnalyzeReturn(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_RETURN);

  DataType_t type = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->ret.value, &type);

  if (a->thisFun->returnType == TYPE_UNKNOWN)
    a->thisFun->returnType = type;

  if (a->thisFun->returnType != type) {
    /* incorrect type error */

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                 "Returning value of type `%s' from a function "
                 "with the return type `%s'.",
                 DataTypeToStr(type), DataTypeToStr(a->thisFun->returnType));
    MsgAddContext(msg, SpanToCtxUnderline(a->thisFun->pos),
                  "function `%s' has return type `%s'",
                  GetIDStr(a->thisFun->id),
                  DataTypeToStr(a->thisFun->returnType));
    MsgAddContext(msg, SpanToCtxUnderline(root->ret.value->pos),
                  "returned vlue is of type `%s'", DataTypeToStr(type));
    MsgSend(msg);
  }

  return err;
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
    return AnalyzeExpr(a, stmnt, NULL);

  default:
    break;
  }

  return AERR_OK;
}

/* RETURN TYPES:
 *   AERR_OK   on success
 *   AERR_MEM  on memory error
 */
static AError_t PopulateBuiltins(Analyzer_t *a) {
  /* Create the builtins `declarations' */
  const struct {
    const char *name;
    DataType_t returnType;

    /* parameters */
    size_t paramCount;
    struct {
      const char *name;
      DataType_t type;
    } params[A_MAX_BUILTINS_PARAMS_COUNT];
  }
#define DO(n, t, p, ...)                                                       \
  {.name = n, .returnType = t, .params = {__VA_ARGS__}, .paramCount = p},
#define P(n, t) {.name = n, .type = t}
  builtins[] = {A_FOR_BUILTINS(DO, P)};
#undef P
#undef DO

#define DO(...) +1
#define P(...)
  const size_t builtinsLen = 0 + A_FOR_BUILTINS(DO, P);
#undef P
#undef DO

  for (size_t i = 0; i < builtinsLen; i++) {
    /* create the symbol*/
    FunSymbol_t *fs = CreateFunSymbol();
    if (!fs)
      return AERR_MEM;

    fs->returnType = builtins[i].returnType;
    fs->isMain = false;
    fs->isDefined = true;
    fs->pos = (SourceSpan_t){0};

    fs->id = CopyStr(builtins[i].name);
    if (!fs->id) {
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    /* add parameters */
    for (size_t j = 0; j < builtins[i].paramCount; j++) {
      /* create the param symbol */
      VarSymbol_t *vs = CreateVarSymbol();
      if (!vs) {
        DeleteFunSymbol(fs);
        return AERR_MEM;
      }

      vs->dtype = builtins[i].params[j].type;
      vs->blockID = A_PARAM_BLOCK;

      vs->id = CopyStr(builtins[i].params[j].name);
      if (!vs->id) {
        DeleteVarSymbol(vs);
        DeleteFunSymbol(fs);
        return AERR_MEM;
      }

      /* add the param symbol */
      if (!SymbolTreeInsertWithData(&fs->symbols, vs->id, vs)) {
        DeleteVarSymbol(vs);
        DeleteFunSymbol(fs);
        return AERR_MEM;
      }
    }

    /* add the declaration */
    if (!SymbolTreeInsertWithData(&a->funs, fs->id, fs)) {
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }
  }

  return AERR_OK;
}
