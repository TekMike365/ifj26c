/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "semantics.h"

#include "bin_tree.h"
#include "int_stack.h"
#include "logger.h"
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
  DO("chr", TYPE_STRING, 1, P("code", TYPE_INT))                               \
  /* ext: BOOLTHEN */                                                          \
  DO("readBoolean", TYPE_BOOLEAN, 0, P("", TYPE_NA))

/*
 * Helper functions
 */

static inline AError_t SetAError(AError_t old, AError_t new) {
  if (new != AERR_OK)
    DLOG_TRACE("<analyzer> Analyzer error is getting set");

  if (old == AERR_MEM)
    return AERR_MEM;
  return old != AERR_OK ? old : new;
}

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

  DLOG_TRACE("<varsym:%p> Variable symbol created.", s);
  return s;
}

static void DeleteVarSymbol(void *data) {
  VarSymbol_t *s = data;
  if (!s)
    return;

  DLOG_TRACE("<varsym:%p> Variable symbol deleted.", s);

  free((void *)s->id);
  free(s);
}

static void DLogVarSymbol(DLogLevel_t lvl, VarSymbol_t *vs, const char *msg) {
  assert(msg);
  if (!vs)
    DebugLog(lvl, "<analyzer> (VarSymbol_t){ NULL }");
  else
    DebugLog(lvl,
             "<analyzer> %s: (VarSymbol_t){ .id = \"%s\", .blockID = %d, "
             ".dtype = %s, .vstate = %d }",
             msg, vs->id ? vs->id : "<NULL>", vs->blockID,
             DataTypeToStr(vs->dtype), vs->vstate);
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
    DLOG_TRACE("<funsym:%p> Failed to create symbol table.", s);
    free(s);
    return NULL;
  }

  DLOG_TRACE("<funsym:%p> Function symbol created.", s);
  return s;
}

static void DLogFunSymbol(DLogLevel_t lvl, FunSymbol_t *fs, const char *msg) {
  assert(msg);
  if (!fs)
    DebugLog(lvl, "<analyzer> (FunSymbol_t){ NULL }");
  else
    DebugLog(lvl,
             "<analyzer> %s: (FunSymbol_t){ .id = \"%s\", "
             ".returnType = %s, .isMain = %s, .isDefined = %s }",
             msg, fs->id ? fs->id : "<NULL>", DataTypeToStr(fs->returnType),
             fs->isMain ? "true" : "false", fs->isDefined ? "true" : "false");
}

static void DeleteFunSymbol(void *data) {
  FunSymbol_t *s = data;
  if (!s)
    return;

  DLOG_TRACE("<funsym:%p> Function symbol deleted.", s);

  free((void *)s->id);
  SymbolTreeDestroy(&s->symbols);
  free(s);
}

/*
 * Analyzer definitions
 */

typedef struct {
  SymbolTree_t funs;
  FunSymbol_t *thisFun;

  IntStack_t *blockStack;
  int blockID;
} Analyzer_t;

static AError_t PopulateBuiltins(Analyzer_t *a);
static void DeleteAnalyzer(Analyzer_t *a);

static Analyzer_t *CreateAnalyzer(void) {
  Analyzer_t *a;
  a = malloc(sizeof *a);
  if (!a)
    return NULL;

  a->thisFun = NULL;
  if (!SymbolTreeInitWithDestructor(&a->funs, DeleteFunSymbol)) {
    DLOG_ERROR("<analyzer:%p> Failed to create function table.", a);
    DeleteAnalyzer(a);
    return NULL;
  }

  a->blockID = 0;
  a->blockStack = IntStackCreate();
  if (!a->blockStack) {
    DLOG_TRACE("<analyzer:%p> Failed to create block stack.", a);
    DeleteAnalyzer(a);
    return NULL;
  }

  /* define the builtins */
  if (AERR_MEM == PopulateBuiltins(a)) {
    DLOG_TRACE("<analyzer:%p> Failed to populate builtins.", a);
    DeleteAnalyzer(a);
    return NULL;
  }

  DLOG_TRACE("<analyzer:%p> Analyzer created.", a);
  return a;
}

static void DeleteAnalyzer(Analyzer_t *a) {
  if (!a)
    return;

  DLOG_TRACE("<analyzer:%p> Analyzer deleted.", a);

  SymbolTreeDestroy(&a->funs);
  IntStackTerminate(a->blockStack);
  free(a);
}

static FunSymbol_t *GetFunSymbol(Analyzer_t *a, const char *id);
static AError_t AddFunParam(FunSymbol_t *fs, const char *name, DataType_t type,
                            SourceSpan_t pos);

static AError_t AddFun(Analyzer_t *a, const ASTNode_t *fndef) {
  FunSymbol_t *fs = GetFunSymbol(a, fndef->fnDef.name);
  if (!fs)
    return AERR_MEM;

  if (fs->isDefined == true) {
    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_REDEF,
                              "Function `%s' already exists.", fs->id);
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "First defined here.");
    MsgSend(msg);

    return AERR_SEM;
  }

  fs->returnType = fndef->fnDef.declType;
  fs->isMain = fndef->fnDef.isMain;
  fs->isDefined = true;
  fs->pos = fndef->pos;

  AError_t err = AERR_OK;

  if (fs->isMain) {
    if (fndef->fnDef.params.count > 0) {
      err = SetAError(err, AERR_SEM);

      Message_t *msg = MsgStart(
          MSG_ERROR, ERROR_SEM_ARGS,
          "Main function must not have arguments `%zu' were specified.",
          fndef->fnDef.params.count);
      MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "defined here");
      MsgSend(msg);
    }
    if (fs->returnType != TYPE_UNIT) {
      err = SetAError(err, AERR_SEM);

      Message_t *msg =
          MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                   "Main function must have return type `%s' got `%s'.",
                   DataTypeToStr(TYPE_UNIT), DataTypeToStr(fs->returnType));
      MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "defined here");
      MsgSend(msg);
    }
  }

  /* Add parameters */
  AST_LIST_FOREACH(fndef->fnDef.params, param) {
    err = SetAError(err, AddFunParam(fs, param->param.name,
                                     param->param.declType, param->pos));
  }

  DLogFunSymbol(DLOG_LVL_INFO, fs, "Added function symbol");

  return err;
}

static AError_t BeginBlock(Analyzer_t *a) {
  assert(a);
  DLOG_INFO("<analyzer> begin block `%d' -> `%d'", a->blockID, a->blockID + 1);
  return IntStackPush(a->blockStack, ++a->blockID) ? AERR_OK : AERR_MEM;
}

static AError_t EndBlock(Analyzer_t *a) {
  assert(a);

  DLOG_INFO("<analyzer> end block `%d'", IntStackTop(a->blockStack));

  IntStackPop(a->blockStack);
  if (IntStackIsEmpty(a->blockStack))
    a->thisFun = NULL;

  return AERR_OK;
}

static AError_t StartFunction(Analyzer_t *a, const ASTNode_t *fndef) {
  assert(a && fndef && fndef->kind == NODE_FNDEF);

  a->thisFun = NULL;
  AError_t err = AERR_OK;

  /* dedent all */
  while (!IntStackIsEmpty(a->blockStack))
    EndBlock(a);
  a->blockID = A_PARAM_BLOCK;

  /* Get function symbol */
  FunSymbol_t *fs = GetFunSymbol(a, fndef->fnDef.name);
  if (!fs)
    return AERR_MEM;

  if (!fs->isDefined)
    err = SetAError(err, AERR_SEM);

  a->thisFun = fs;

  DLogFunSymbol(DLOG_LVL_INFO, fs, "<analyzer> start function");
  return err;
}

/*
 * Expression analysis
 */

static AError_t AnalyzeBinOp(Analyzer_t *a, ASTNode_t *root, DataType_t *type);
static AError_t AnalyzeUnaryOp(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type);
static AError_t AnalyzeFunCall(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type);
static AError_t AnalyzeLiteral(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type);
static AError_t AnalyzeExpr(Analyzer_t *a, ASTNode_t *root, DataType_t *type);

/* returns NULL if not found (or error) */
static VarSymbol_t *GetActiveVSymbol(Analyzer_t *a, const char *id) {
  assert(a && id);

  long vsDepth = -1;
  VarSymbol_t *vs = NULL;

  SymbolTreeIndexNode_t *indexlist;
  SymbolTreeFindAllIndices(&a->thisFun->symbols, id, &indexlist);

  SymbolTreeIndexNode_t *symbol = indexlist;
  for (; symbol; symbol = symbol->next) {
    const SymbolHeader_t *sh =
        SymbolTreeAt(&a->thisFun->symbols, symbol->index);
    VarSymbol_t *s = (VarSymbol_t *)sh->data;

    if (s->blockID == A_PARAM_BLOCK) {
      vsDepth = A_PARAM_BLOCK;
      vs = s;
      break;
    }

    long d = IntStackDepth(a->blockStack, s->blockID);

    if (vsDepth < 0 || (d < vsDepth && d >= 0)) {
      vsDepth = d;
      vs = s;
    }
  }

  SymbolTreeIndexListDestroy(indexlist);

  return vs;
}

/* returns NULL if not found (or error) */
static FunSymbol_t *GetFunSymbol(Analyzer_t *a, const char *id) {
  assert(a && id);

  SymbolHeader_t *sh = SymbolTreeSearchMutable(&a->funs, id);
  if (sh)
    return (FunSymbol_t *)sh->data;

  /* create the symbol */
  FunSymbol_t *fs = CreateFunSymbol();
  if (!fs)
    return NULL;

  fs->returnType = TYPE_UNKNOWN;
  fs->isMain = false;
  fs->isDefined = false;
  fs->pos = (SourceSpan_t){0};

  fs->id = CopyStr(id);
  if (!fs->id) {
    DLOG_ERROR("<analyzer> id copy malloc error");
    DeleteFunSymbol(fs);
    return NULL;
  }

  /* add function symbol */
  if (!SymbolTreeInsertWithData(&a->funs, fs->id, fs)) {
    DLogFunSymbol(DLOG_LVL_ERROR, fs, "Failed to save function");
    DeleteFunSymbol(fs);
    return NULL;
  }

  DLogFunSymbol(DLOG_LVL_INFO, fs, "Added function");

  return fs;
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
                DataTypeToStr(ltype));
  MsgAddContext(msg, SpanToCtxUnderline(rpos), "typeof right expression: `%s'",
                DataTypeToStr(rtype));
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
  assert(a && root && (root->kind == NODE_BINOP || root->kind == NODE_COND));

  AError_t err = AERR_OK;
  DataType_t otype = TYPE_UNKNOWN;

  /* get subexpression types */
  DataType_t ltype = TYPE_UNKNOWN;
  DataType_t rtype = TYPE_UNKNOWN;
  err = SetAError(err, AnalyzeExpr(a, root->binOp.left, &ltype));
  err = SetAError(err, AnalyzeExpr(a, root->binOp.right, &rtype));

  SourceSpan_t lpos = root->binOp.left->pos;
  SourceSpan_t rpos = root->binOp.right->pos;

  bool isLMath = ltype == TYPE_INT || ltype == TYPE_DOUBLE;
  bool isRMath = ltype == TYPE_INT || ltype == TYPE_DOUBLE;

  switch (root->binOp.op) {
  /* maths */
  case OP_DIV:
  case OP_MUL:
  case OP_MINUS: {
    if (!isLMath || !isRMath) {
      /* type error */
      err = SetAError(err, AERR_SEM);
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                     &otype);
  } break;

  /* special */
  case OP_PLUS: {
    if ((!isLMath || !isRMath) &&
        (ltype != TYPE_STRING || rtype != TYPE_STRING)) {
      /* type error */
      err = SetAError(err, AERR_SEM);
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
    if ((!isLMath || !isRMath) &&
        (ltype != TYPE_STRING || rtype != TYPE_STRING)) {
      /* type error */
      err = SetAError(err, AERR_SEM);
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
    if ((!isLMath || !isRMath) && (ltype != rtype)) {
      /* type error */
      err = SetAError(err, AERR_SEM);
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    if (ltype != rtype)
      CorrectMathTypes(&root->binOp.left, &root->binOp.right, ltype, rtype,
                       &otype);

    otype = TYPE_BOOLEAN;
  } break;

  /* ext: BOOLTHEN */
  case OP_AND:
  case OP_OR: {
    if (ltype != TYPE_BOOLEAN || rtype != TYPE_BOOLEAN) {
      /* type error */
      err = SetAError(err, AERR_SEM);
      ReportBinOpValueError(OpToChar(root->binOp.op), ltype, rtype, root->pos,
                            lpos, rpos);
      break;
    }

    otype = TYPE_BOOLEAN;
  } break;

  /* default */
  default:
    break;
  }

  /* set type  */
  root->type = otype;

  if (*type == TYPE_UNKNOWN)
    *type = otype;

  if (*type == TYPE_UNKNOWN) {
    /* type error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(
        MSG_ERROR, ERROR_SEM_TYPE,
        "Unknown types resulted from operation `%s': left `%s', right `%s'.",
        OpToChar(root->binOp.op), DataTypeToStr(ltype), DataTypeToStr(rtype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type != otype) {
    /* type error */
    err = SetAError(err, AERR_SEM);

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

  DataType_t otype = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->unOp.operand, &otype);

  switch (root->unOp.op) {
  case OP_UMINUS:
  case OP_UPLUS: {
    if (otype == TYPE_INT || otype == TYPE_DOUBLE)
      break;

    /* type error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                              "Type error: expected `%s' or `%s' != got `%s'",
                              DataTypeToStr(TYPE_INT),
                              DataTypeToStr(TYPE_DOUBLE), DataTypeToStr(otype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgAddContext(msg, SpanToCtxUnderline(root->unOp.operand->pos),
                  "typeof expression is `%s'", DataTypeToStr(otype));
    MsgSend(msg);
  } break;

  /* ext: BOOLTHEN */
  case OP_NEG: {
    if (otype == TYPE_BOOLEAN)
      break;

    /* type error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(
        MSG_ERROR, ERROR_SEM_TYPE, "Type error: expected `%s' != got `%s'",
        DataTypeToStr(TYPE_BOOLEAN), DataTypeToStr(otype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgAddContext(msg, SpanToCtxUnderline(root->unOp.operand->pos),
                  "typeof expression is `%s'", DataTypeToStr(otype));
    MsgSend(msg);
  } break;

  default:
    break;
  }

  /* set type  */
  root->type = otype;

  if (*type == TYPE_UNKNOWN)
    *type = otype;

  if (*type == TYPE_UNKNOWN) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE, "Unknown unary operation type");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type != otype) {
    /* type error */
    err = SetAError(err, AERR_SEM);

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
static AError_t AnalyzeFunCall(Analyzer_t *a, ASTNode_t *root,
                               DataType_t *type) {
  assert(a && root && root->kind == NODE_FNCALL);

  FunSymbol_t *fs = GetFunSymbol(a, root->name);
  if (!fs)
    return AERR_MEM;

  if (!fs->isDefined) {
    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                              "Called function `%s' is undefined.", fs->id);
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "first appeared here");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "called here");
    MsgSend(msg);
    return AERR_SEM;
  }

  AError_t err = AERR_OK;

  /* check arguments */
  const SymbolHeader_t *sh;
  ASTNode_t **expr = root->fncall.args.items;
  const size_t exprCnt = root->fncall.args.count;
  size_t i;
  for (i = 0; (sh = SymbolTreeAt(&fs->symbols, i)) && i < exprCnt; i++) {
    VarSymbol_t *vs = (VarSymbol_t *)sh->data;

    DataType_t type = TYPE_UNKNOWN;
    err = SetAError(err, AnalyzeExpr(a, expr[i], &type));

    if (vs->dtype != type && vs->dtype != TYPE_ANY) {
      /* type error */
      Message_t *msg = MsgStart(
          MSG_ERROR, ERROR_SEM_ARGS,
          "Unexpected type for argument `%s' (type `%s') of function `%s' "
          "and expression (type `%s').",
          vs->id, DataTypeToStr(vs->dtype), fs->id, DataTypeToStr(type));
      MsgAddContext(msg, SpanToCtxUnderline(expr[i]->pos), "here");
      MsgAddContext(msg, SpanToCtxUnderline(fs->pos),
                    "function `%s' defined here", fs->id);
      MsgSend(msg);
    }
  }

  if (i != exprCnt) {
    /* too many arguments */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_ARGS,
                              "Too many arguments for function `%s'.", fs->id);
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "called here");
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "defined here");
    MsgSend(msg);
  }

  if (sh && ((VarSymbol_t *)sh->data)->blockID == A_PARAM_BLOCK) {
    /* not enough arguments */
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_ARGS,
                 "Not enough arguments for function `%s'.", fs->id);
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "called here");
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "defined here");
    MsgSend(msg);
  }

  /* set type */
  root->type = fs->returnType;

  if (*type == TYPE_UNKNOWN)
    *type = fs->returnType;

  if (*type == TYPE_UNKNOWN) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE, "Unknown function return type");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type != fs->returnType) {
    /* incompatible return type */
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE,
                 "Incompatible return type: returned `%s' expected `%s'.",
                 DataTypeToStr(fs->returnType), DataTypeToStr(*type));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "defined here");
    MsgSend(msg);
  }

  return err;
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

  if (*type == TYPE_UNKNOWN) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE, "Unknown literal type");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type != ltype) {
    /* incompatible types */
    err = SetAError(err, AERR_SEM);

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
static AError_t AnalyzeID(Analyzer_t *a, ASTNode_t *root, DataType_t *type) {
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
      DLOG_ERROR("<analyzer> id copy malloc error");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    if (!SymbolTreeInsertWithData(&a->thisFun->symbols, vs->id, vs)) {
      DLogVarSymbol(DLOG_LVL_ERROR, vs, "Failed to save variable symbol");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    DLogVarSymbol(DLOG_LVL_INFO, vs, "Created variable symbol");
  }

  /* set the type */
  root->type = vs->dtype;

  AError_t err = AERR_OK;

  if (vs->vstate == VS_UNDEFINED) {
    /* undefined variable error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                              "Variable `%s' is used but not defined.", vs->id);
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first appeared here");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type == TYPE_UNKNOWN)
    *type = vs->dtype;

  if (*type == TYPE_UNKNOWN) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_TYPE, "Unknown ID type");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  if (*type != vs->dtype) {
    /* type error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(
        MSG_ERROR, ERROR_SEM_TYPE,
        "Variable `%s' is of uncompatible type. Expected `%s' got `%s'.",
        vs->id, DataTypeToStr(*type), DataTypeToStr(vs->dtype));
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
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
  DataType_t dt = TYPE_UNKNOWN;

  switch (root->kind) {
  case NODE_UNOP:
    err = SetAError(err, AnalyzeUnaryOp(a, root, &dt));
    break;
  case NODE_BINOP:
  case NODE_COND:
    err = SetAError(err, AnalyzeBinOp(a, root, &dt));
    break;
  case NODE_FNCALL:
    err = SetAError(err, AnalyzeFunCall(a, root, &dt));
    break;
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
    err = SetAError(err, AnalyzeLiteral(a, root, &dt));
    break;
  case NODE_ID:
    err = SetAError(err, AnalyzeID(a, root, &dt));
    break;
  default: {
    Message_t *msg = MsgStart(MSG_INFO, ERROR_SEM_TYPE, "Unknown expression");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "here");
    MsgSend(msg);
  } break;
  }

  if (dt == TYPE_UNKNOWN) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_TYPE, "Unknown expression type");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "used here");
    MsgSend(msg);
  }

  /* set type  */
  root->type = dt;

  if (type)
    *type = dt;

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
      DLOG_ERROR("<analyzer> id copy malloc error");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    if (!SymbolTreeInsertWithData(&a->thisFun->symbols, vs->id, vs)) {
      DLogVarSymbol(DLOG_LVL_ERROR, vs, "Failed to save variable symbol");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    DLogVarSymbol(DLOG_LVL_INFO, vs, "Created variable symbol");
  }

  if (vs->vstate == VS_UNDEFINED) {
    /* undefined variable error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                 "Variable `%s' is assigned to but it's not defined.",
                 root->assign.name);
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first appeared here");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "assigned here");
    MsgSend(msg);
  } else if (vs->vstate == VS_CONST) {
    /* assignment to a constant error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_IMMUTABLE,
                 "Assignment to a immutable variable `%s'.", root->assign.name);
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first appeared here");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "assigned here");
    MsgSend(msg);
  }

  /* expr evaluation & type check */
  DataType_t type = TYPE_UNKNOWN;
  err = SetAError(err, AnalyzeExpr(a, root->assign.value, &type));

  /* set type  */
  root->type = type;

  if (vs->dtype == TYPE_UNKNOWN)
    vs->dtype = type;

  if (vs->dtype != type) {
    /* incorrect type error */
    err = SetAError(err, AERR_SEM);

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
  assert(a && root && (root->kind == NODE_VARDEF || root->kind == NODE_VALDEF));

  AError_t err = AERR_OK;

  VarSymbol_t *vs = GetActiveVSymbol(a, root->varDef.name);

  if (vs && vs->blockID == a->blockID && vs->vstate != VS_UNDEFINED) {
    /* redefinition error */
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_REDEF, "Variable `%s' already exists.",
                 root->varDef.name);
    MsgAddContext(msg, SpanToCtxUnderline(vs->pos), "first defined here");
    MsgAddContext(msg, SpanToCtxUnderline(root->pos), "redefined here");
    MsgSend(msg);

    /* process like it is assignment */
  }

  /* create if it doesn't exist*/
  bool created = false;
  if (vs == NULL) {
    created = true;

    vs = CreateVarSymbol();
    if (!vs)
      return AERR_MEM;

    vs->blockID = a->blockID;
    vs->dtype = root->varDef.declType;
    vs->vstate = VS_MUTABLE;
    vs->pos = root->pos;

    vs->id = CopyStr(root->varDef.name);
    if (!vs->id) {
      DLOG_ERROR("<analyzer> id copy malloc error");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    if (!SymbolTreeInsertWithData(&a->thisFun->symbols, vs->id, vs)) {
      DLogVarSymbol(DLOG_LVL_ERROR, vs, "Failed to save variable symbol");
      DeleteVarSymbol(vs);
      return AERR_MEM;
    }

    DLogVarSymbol(DLOG_LVL_INFO, vs, "Created variable symbol");
  }

  /* treat as assignment */
  ASTNode_t assign = *root;
  assign.kind = NODE_ASSIGN;
  assign.assign.value = root->varDef.init;

  err = SetAError(err, AnalyzeAssign(a, &assign));

  /* set type  */
  root->type = vs->dtype;
  root->varDef.declType = vs->dtype;

  if (created)
    vs->vstate = isMutable ? VS_MUTABLE : VS_CONST;

  return err;
}

static AError_t AnalyzeIf(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root && root->kind == NODE_IF);

  DataType_t type = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->ifElse.cond, &type);

  if (type != TYPE_BOOLEAN) {
    /* incorrect type error */
    err = SetAError(err, AERR_SEM);

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

  DataType_t type = TYPE_UNKNOWN;
  AError_t err = AnalyzeExpr(a, root->whileDo.cond, &type);

  if (type != TYPE_BOOLEAN) {
    /* incorrect type error */
    err = SetAError(err, AERR_SEM);

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

  /* set type  */
  root->type = type;

  if (a->thisFun->returnType == TYPE_UNKNOWN)
    a->thisFun->returnType = type;

  if (a->thisFun->returnType != type) {
    /* incorrect type error */
    err = SetAError(err, AERR_SEM);

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
                  "returned value is of type `%s'", DataTypeToStr(type));
    MsgSend(msg);
  }

  return err;
}

static AError_t AnalyzeStatement(Analyzer_t *a, ASTNode_t *stmnt) {
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

  DLOG_INFO("<analyzer> Populating builtins.");

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
      DLOG_ERROR("<analyzer> id copy malloc error");
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    /* add parameters */
    AError_t err = AERR_OK;
    for (size_t j = 0; j < builtins[i].paramCount; j++) {
      err = SetAError(err, AddFunParam(fs, builtins[i].params[j].name,
                                       builtins[i].params[j].type,
                                       (SourceSpan_t){0}));
    }
    if (err == AERR_MEM) {
      DLogFunSymbol(DLOG_LVL_ERROR, fs, "Failed to create builtins parameters");
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    /* add the declaration */
    if (!SymbolTreeInsertWithData(&a->funs, fs->id, fs)) {
      DLogFunSymbol(DLOG_LVL_ERROR, fs, "Failed to save builtins symbol");
      DeleteFunSymbol(fs);
      return AERR_MEM;
    }

    DLogFunSymbol(DLOG_LVL_INFO, fs, "Created builtins function symbol");
  }

  return AERR_OK;
}

static AError_t AddFunParam(FunSymbol_t *fs, const char *name, DataType_t type,
                            SourceSpan_t pos) {
  VarSymbol_t *vs = CreateVarSymbol();
  if (!vs)
    return AERR_MEM;

  vs->dtype = type;
  vs->vstate = VS_MUTABLE;
  vs->blockID = A_PARAM_BLOCK;
  vs->pos = pos;

  vs->id = CopyStr(name);
  if (!vs->id) {
    DLOG_ERROR("<analyzer> id copy malloc error");
    DeleteFunSymbol(vs);
    return AERR_MEM;
  }

  /* check if the parameter has a type (sanity check) */
  assert(vs->dtype != TYPE_UNKNOWN && vs->dtype != TYPE_NA);

  if (!SymbolTreeInsertWithData(&fs->symbols, vs->id, vs)) {
    DLogVarSymbol(DLOG_LVL_INFO, vs, "Failed to create function parameter");
    DeleteVarSymbol(vs);
    return AERR_MEM;
  }

  DLogVarSymbol(DLOG_LVL_INFO, vs, "Added parameter");

  return AERR_OK;
}

static AError_t AnalyzeSubAST(Analyzer_t *a, ASTNode_t *root) {
  assert(a && root);

  if (!root) {
    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_INTERNAL, "Empty AST node encountered.");
    MsgSend(msg);
    return AERR_SEM;
  }

  AError_t err = AERR_OK;

  switch (root->kind) {
  case NODE_FNDEF: {
    err = SetAError(err, StartFunction(a, root));
    err = SetAError(err, AnalyzeSubAST(a, root->fnDef.body));
  } break;
  case NODE_IF: {
    err = SetAError(err, AnalyzeStatement(a, root));
    err = SetAError(err, AnalyzeSubAST(a, root->ifElse.thenBlock));
    if (root->ifElse.elseBlock)
      err = SetAError(err, AnalyzeSubAST(a, root->ifElse.elseBlock));
  } break;
  case NODE_WHILE: {
    err = SetAError(err, AnalyzeStatement(a, root));
    err = SetAError(err, AnalyzeSubAST(a, root->whileDo.body));
  } break;
  case NODE_BLOCK: {
    err = SetAError(err, BeginBlock(a));
    AST_LIST_FOREACH(root->block, stmnt) {
      err = SetAError(err, AnalyzeSubAST(a, stmnt));
    }
    err = SetAError(err, EndBlock(a));
  } break;
  default:
    err = SetAError(err, AnalyzeStatement(a, root));
  }

  return err;
}

static AError_t CheckMainDefs(Analyzer_t *a) {
  AError_t err = AERR_OK;

  bool seenMain = false;
  FunSymbol_t *main = NULL;

  const SymbolHeader_t *sh;
  for (size_t i = 0; (sh = SymbolTreeAt(&a->funs, i)); i++) {
    FunSymbol_t *fs = (FunSymbol_t *)sh->data;

    if (!fs->isMain)
      continue;

    if (!seenMain && fs->isMain) {
      seenMain = true;
      main = fs;
      continue;
    }

    /* multiple main function definitions */
    err = SetAError(err, AERR_SEM);

    Message_t *msg = MsgStart(MSG_ERROR, ERROR_SEM_REDEF,
                              "Multiple definitions of `@main'.");
    MsgAddContext(msg, SpanToCtxUnderline(main->pos),
                  "first defined here (`%s')", main->id);
    MsgAddContext(msg, SpanToCtxUnderline(fs->pos), "redefined here (`%s')",
                  fs->id);
    MsgSend(msg);
  }

  if (!seenMain) {
    err = SetAError(err, AERR_SEM);

    Message_t *msg =
        MsgStart(MSG_ERROR, ERROR_SEM_UNDEF,
                 "How am I supposed to run without a main function? Where am I "
                 "to start? !! Missing `@main' function. !!");
    MsgSend(msg);
  }

  return err;
}

AError_t AnalyzeAST(ASTNode_t *root) {
  assert(root && root->kind == NODE_PROGRAM);

  Analyzer_t *a = CreateAnalyzer();
  if (!a) {
    DLOG_ERROR("<analyzer> failed to create analyzer");
    return AERR_MEM;
  }

  /* add all the definitions */
  AError_t err = AERR_OK;
  AST_LIST_FOREACH(root->program, def) { err = SetAError(err, AddFun(a, def)); }

  /* recurse */
  AST_LIST_FOREACH(root->program, def) {
    err = SetAError(err, AnalyzeSubAST(a, def));
  }

  err = SetAError(err, CheckMainDefs(a));

  DeleteAnalyzer(a);

  return err;
}
