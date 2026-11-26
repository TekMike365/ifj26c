/* Implementace imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#include "ifjcode26.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct LoopInfo {
  const ASTNode_t *loop;
  size_t endId;
  size_t stepId;
} LoopInfo_t;

typedef struct FunctionInfo {
  const ASTNode_t *node;
  size_t id;
  size_t localCount;
  struct LocalInfo *locals;
  LoopInfo_t *loops;
  size_t loopCount;
} FunctionInfo_t;

typedef struct LocalInfo {
  const ASTNode_t *declaration;
  size_t id;
  DataType_t type;
} LocalInfo_t;

typedef struct Binding {
  const char *name;
  DataType_t type;
  size_t id;
  bool isParameter;
} Binding_t;

typedef struct Codegen {
  FILE *output;
  bool failed;
  size_t nextLabel;
  FunctionInfo_t *functions;
  size_t functionCount;
  const FunctionInfo_t *currentFunction;
  Binding_t *bindings;
  size_t bindingCount;
  size_t bindingCapacity;
  size_t nextLocalId;
} Codegen_t;

/* Write one formatted instruction fragment and remember output errors. */
static bool Emit(Codegen_t *codegen, const char *format, ...) {
  if (codegen->failed) {
    return false;
  }

  va_list args;
  va_start(args, format);
  if (vfprintf(codegen->output, format, args) < 0) {
    codegen->failed = true;
  }
  va_end(args);
  return !codegen->failed;
}

/* Return whether this AST node is one of the expression kinds we support. */
static bool IsExpression(const ASTNode_t *node) {
  if (node == NULL) {
    return false;
  }

  switch (node->kind) {
  case NODE_UNOP:
  case NODE_BINOP:
  case NODE_COND:
  case NODE_FNCALL:
  case NODE_IFEXPR:
  case NODE_CONV_DOUBLE:
  case NODE_ID:
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
    return true;
  default:
    return false;
  }
}

/* Look up a user-defined function by its source-level name. */
static const FunctionInfo_t *FindFunction(const Codegen_t *codegen,
                                          const char *name) {
  for (size_t i = 0; i < codegen->functionCount; i++) {
    const ASTNode_t *function = codegen->functions[i].node;
    if (strcmp(function->fnDef.name, name) == 0) {
      return &codegen->functions[i];
    }
  }
  return NULL;
}

/* Find the nearest visible variable or parameter with this name. */
static const Binding_t *FindBinding(const Codegen_t *codegen,
                                    const char *name) {
  for (size_t i = codegen->bindingCount; i > 0; i--) {
    if (strcmp(codegen->bindings[i - 1].name, name) == 0) {
      return &codegen->bindings[i - 1];
    }
  }
  return NULL;
}

/* Add a variable or parameter to the current lexical-scope lookup table. */
static bool AddBinding(Codegen_t *codegen, const char *name, DataType_t type,
                       size_t id, bool isParameter) {
  if (name == NULL) {
    return false;
  }

  if (codegen->bindingCount == codegen->bindingCapacity) {
    size_t newCapacity =
        codegen->bindingCapacity == 0 ? 8 : codegen->bindingCapacity * 2;
    if (newCapacity < codegen->bindingCapacity ||
        newCapacity > SIZE_MAX / sizeof *codegen->bindings) {
      return false;
    }

    Binding_t *newBindings =
        realloc(codegen->bindings, newCapacity * sizeof *newBindings);
    if (newBindings == NULL) {
      return false;
    }
    codegen->bindings = newBindings;
    codegen->bindingCapacity = newCapacity;
  }

  codegen->bindings[codegen->bindingCount++] =
      (Binding_t){.name = name,
                  .type = type,
                  .id = id,
                  .isParameter = isParameter};
  return true;
}

/* Get an expression's known type, inferring basic types when needed. */
static DataType_t ExpressionType(const Codegen_t *codegen,
                                 const ASTNode_t *node) {
  if (node == NULL) {
    return TYPE_UNKNOWN;
  }

  if (node->type != TYPE_UNKNOWN && node->type != TYPE_NA) {
    return node->type;
  }

  switch (node->kind) {
  case NODE_INT:
    return TYPE_INT;
  case NODE_DOUBLE:
    return TYPE_DOUBLE;
  case NODE_STRING:
    return TYPE_STRING;
  case NODE_UNIT:
    return TYPE_UNIT;
  case NODE_CONV_DOUBLE:
    return TYPE_DOUBLE;
  case NODE_ID: {
    const Binding_t *binding = FindBinding(codegen, node->name);
    return binding == NULL ? TYPE_UNKNOWN : binding->type;
  }
  case NODE_FNCALL: {
    if (node->fncall.name == NULL) {
      return TYPE_UNKNOWN;
    }
    const FunctionInfo_t *function =
        FindFunction(codegen, node->fncall.name);
    if (function != NULL) {
      return function->node->fnDef.declType;
    }
    if (strcmp(node->fncall.name, "print") == 0) {
      return TYPE_UNIT;
    }
    if (strcmp(node->fncall.name, "readLine") == 0 ||
        strcmp(node->fncall.name, "toStr") == 0 ||
        strcmp(node->fncall.name, "subStr") == 0 ||
        strcmp(node->fncall.name, "chr") == 0) {
      return TYPE_STRING;
    }
    if (strcmp(node->fncall.name, "readInt") == 0 ||
        strcmp(node->fncall.name, "toInt") == 0 ||
        strcmp(node->fncall.name, "strLength") == 0 ||
        strcmp(node->fncall.name, "ord") == 0) {
      return TYPE_INT;
    }
    if (strcmp(node->fncall.name, "readDouble") == 0) {
      return TYPE_DOUBLE;
    }
    if (strcmp(node->fncall.name, "readBoolean") == 0) {
      return TYPE_BOOLEAN;
    }
    return TYPE_UNKNOWN;
  }
  case NODE_UNOP:
    if (node->unOp.op == OP_NEG) {
      return TYPE_BOOLEAN;
    }
    return ExpressionType(codegen, node->unOp.operand);
  case NODE_BINOP:
  case NODE_COND:
    if (node->binOp.op == OP_EQ || node->binOp.op == OP_NEQ ||
        node->binOp.op == OP_LTE || node->binOp.op == OP_LT ||
        node->binOp.op == OP_GTE || node->binOp.op == OP_GT ||
        node->binOp.op == OP_AND || node->binOp.op == OP_OR) {
      return TYPE_BOOLEAN;
    }
    if (node->binOp.op == OP_PLUS &&
        (ExpressionType(codegen, node->binOp.left) == TYPE_STRING ||
         ExpressionType(codegen, node->binOp.right) == TYPE_STRING)) {
      return TYPE_STRING;
    }
    if (ExpressionType(codegen, node->binOp.left) == TYPE_DOUBLE ||
        ExpressionType(codegen, node->binOp.right) == TYPE_DOUBLE) {
      return TYPE_DOUBLE;
    }
    return TYPE_INT;
  case NODE_IFEXPR: {
    DataType_t thenType = ExpressionType(codegen, node->ifexpr.thenExpr);
    DataType_t elseType = ExpressionType(codegen, node->ifexpr.elseExpr);
    if (thenType == elseType) {
      return thenType;
    }
    if ((thenType == TYPE_INT && elseType == TYPE_DOUBLE) ||
        (thenType == TYPE_DOUBLE && elseType == TYPE_INT)) {
      return TYPE_DOUBLE;
    }
    return TYPE_UNKNOWN;
  }
  default:
    return TYPE_UNKNOWN;
  }
}

/* Convert the value on top of the data stack when the target type requires it. */
static bool ConvertStackValue(Codegen_t *codegen, DataType_t from,
                              DataType_t to) {
  if (from == to || to == TYPE_UNKNOWN || to == TYPE_NA) {
    return true;
  }
  if (from == TYPE_INT && to == TYPE_DOUBLE) {
    return Emit(codegen, "INT2FLOATS\n");
  }
  return from == TYPE_UNKNOWN;
}

/* Record a local declaration and the frame slot assigned to it. */
static bool AppendLocal(FunctionInfo_t *function, const ASTNode_t *decl,
                        size_t id, DataType_t type) {
  if (function->localCount >= SIZE_MAX / sizeof *function->locals) {
    return false;
  }
  LocalInfo_t *locals =
      realloc(function->locals, (function->localCount + 1) * sizeof *locals);
  if (locals == NULL) {
    return false;
  }
  function->locals = locals;
  function->locals[function->localCount++] =
      (LocalInfo_t){.declaration = decl, .id = id, .type = type};
  return true;
}

/* Find the recorded frame slot belonging to a declaration node. */
static const LocalInfo_t *FindLocal(const FunctionInfo_t *function,
                                    const ASTNode_t *decl) {
  for (size_t i = 0; i < function->localCount; i++) {
    if (function->locals[i].declaration == decl) {
      return &function->locals[i];
    }
  }
  return NULL;
}

static bool AppendLoopInfo(FunctionInfo_t *function, const ASTNode_t *loop,
                           size_t endId, size_t stepId) {
  if (function->loopCount >= SIZE_MAX / sizeof *function->loops) {
    return false;
  }
  LoopInfo_t *loops =
      realloc(function->loops, (function->loopCount + 1) * sizeof *loops);
  if (loops == NULL) {
    return false;
  }
  function->loops = loops;
  function->loops[function->loopCount++] =
      (LoopInfo_t){.loop = loop, .endId = endId, .stepId = stepId};
  return true;
}

static const LoopInfo_t *FindLoopInfo(const FunctionInfo_t *function,
                                      const ASTNode_t *loop) {
  for (size_t i = 0; i < function->loopCount; i++) {
    if (function->loops[i].loop == loop) {
      return &function->loops[i];
    }
  }
  return NULL;
}

static bool CollectExpression(Codegen_t *codegen, const ASTNode_t *node);

/* Pre-scan a block to assign local slots while respecting nested scopes. */
static bool CollectBlock(Codegen_t *codegen, FunctionInfo_t *function,
                         const ASTNode_t *block) {
  if (block == NULL || block->kind != NODE_BLOCK ||
      (block->block.count != 0 && block->block.items == NULL)) {
    return false;
  }

  size_t scopeStart = codegen->bindingCount;
  for (size_t i = 0; i < block->block.count; i++) {
    const ASTNode_t *statement = block->block.items[i];
    if (statement == NULL) {
      codegen->bindingCount = scopeStart;
      return false;
    }

    switch (statement->kind) {
    case NODE_VALDEF:
    case NODE_VARDEF: {
      if (!CollectExpression(codegen, statement->varDef.init)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      DataType_t type = statement->varDef.declType;
      if (type == TYPE_UNKNOWN) {
        type = ExpressionType(codegen, statement->varDef.init);
      }
      size_t id = codegen->nextLocalId++;
      if (!AppendLocal(function, statement, id, type) ||
          !AddBinding(codegen, statement->varDef.name, type, id, false)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    }
    case NODE_ASSIGN:
      if (statement->assign.name == NULL ||
          !CollectExpression(codegen, statement->assign.value)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    case NODE_IF:
      if (!CollectExpression(codegen, statement->ifElse.cond) ||
          !CollectBlock(codegen, function, statement->ifElse.thenBlock) ||
          (statement->ifElse.elseBlock != NULL &&
           !CollectBlock(codegen, function, statement->ifElse.elseBlock))) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    case NODE_WHILE:
      if (!CollectExpression(codegen, statement->whileDo.cond) ||
          !CollectBlock(codegen, function, statement->whileDo.body)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    case NODE_FOR: {
      const ASTNode_t *generator = statement->forLoop.gen;
      if (generator == NULL || generator->kind != NODE_GEN ||
          generator->gen.name == NULL || generator->gen.from == NULL ||
          generator->gen.to == NULL || statement->forLoop.body == NULL ||
          !CollectExpression(codegen, generator->gen.from) ||
          !CollectExpression(codegen, generator->gen.to) ||
          (generator->gen.by != NULL &&
           !CollectExpression(codegen, generator->gen.by)) ||
          ExpressionType(codegen, generator->gen.from) != TYPE_INT ||
          ExpressionType(codegen, generator->gen.to) != TYPE_INT ||
          (generator->gen.by != NULL &&
           ExpressionType(codegen, generator->gen.by) != TYPE_INT) ||
          codegen->nextLocalId > SIZE_MAX - 3) {
        codegen->bindingCount = scopeStart;
        return false;
      }

      size_t valueId = codegen->nextLocalId++;
      size_t endId = codegen->nextLocalId++;
      size_t stepId = codegen->nextLocalId++;
      if (!AppendLocal(function, generator, valueId, TYPE_INT) ||
          !AppendLocal(function, statement, endId, TYPE_INT) ||
          !AppendLocal(function, statement->forLoop.body, stepId, TYPE_INT) ||
          !AppendLoopInfo(function, statement, endId, stepId) ||
          !AddBinding(codegen, generator->gen.name, TYPE_INT, valueId, false) ||
          !CollectBlock(codegen, function, statement->forLoop.body)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      codegen->bindingCount = scopeStart;
      break;
    }
    case NODE_RETURN:
      if (statement->ret.value != NULL &&
          !CollectExpression(codegen, statement->ret.value)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    case NODE_BLOCK:
      if (!CollectBlock(codegen, function, statement)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    default:
      if (!CollectExpression(codegen, statement)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
      break;
    }
  }

  codegen->bindingCount = scopeStart;
  return true;
}

/* Check that an expression tree contains only supported, well-formed nodes. */
static bool CollectExpression(Codegen_t *codegen, const ASTNode_t *node) {
  if (node == NULL) {
    return false;
  }
  switch (node->kind) {
  case NODE_ID:
    return node->name != NULL;
  case NODE_INT:
  case NODE_DOUBLE:
  case NODE_STRING:
  case NODE_UNIT:
    return node->kind != NODE_STRING || node->string != NULL;
  case NODE_CONV_DOUBLE:
    return CollectExpression(codegen, node->value);
  case NODE_UNOP:
    return CollectExpression(codegen, node->unOp.operand);
  case NODE_BINOP:
  case NODE_COND:
    return CollectExpression(codegen, node->binOp.left) &&
           CollectExpression(codegen, node->binOp.right);
  case NODE_IFEXPR:
    return CollectExpression(codegen, node->ifexpr.cond) &&
           CollectExpression(codegen, node->ifexpr.thenExpr) &&
           CollectExpression(codegen, node->ifexpr.elseExpr);
  case NODE_FNCALL:
    if (node->fncall.name == NULL ||
        (node->fncall.args.count != 0 && node->fncall.args.items == NULL)) {
      return false;
    }
    for (size_t i = 0; i < node->fncall.args.count; i++) {
      if (!CollectExpression(codegen, node->fncall.args.items[i])) {
        return false;
      }
    }
    return true;
  default:
    return false;
  }
}

/* Escape characters that are special in IFJcode string literals. */
static bool EscapeString(Codegen_t *codegen, const char *string) {
  if (!Emit(codegen, "string@")) {
    return false;
  }
  for (const unsigned char *p = (const unsigned char *)string; *p != '\0';
       p++) {
    if (*p <= 32 || *p == '#' || *p == '\\') {
      if (!Emit(codegen, "\\%03u", (unsigned int)*p)) {
        return false;
      }
    } else if (!Emit(codegen, "%c", *p)) {
      return false;
    }
  }
  return true;
}

static bool EmitExpression(Codegen_t *codegen, const ASTNode_t *node);
static bool EmitStatement(Codegen_t *codegen, const ASTNode_t *node);

/* Inline the IFJcode26 implementation of a supported built-in function. */
static bool EmitBuiltinCall(Codegen_t *codegen, const ASTNode_t *node,
                            DataType_t *resultType) {
  const char *name = node->fncall.name;
  const ASTList_t *args = &node->fncall.args;

  if (strcmp(name, "print") == 0) {
    if (args->count != 1 || !EmitExpression(codegen, args->items[0])) {
      return false;
    }
    DataType_t type = ExpressionType(codegen, args->items[0]);
    if (!Emit(codegen, "POPS GF@_tmp0\n")) {
      return false;
    }
    if (type == TYPE_UNIT) {
      if (!Emit(codegen, "WRITE string@()\n")) {
        return false;
      }
    } else if (!Emit(codegen, "WRITE GF@_tmp0\n")) {
      return false;
    }
    *resultType = TYPE_UNIT;
    return Emit(codegen, "PUSHS nil@nil\n");
  }

  if (args->count == 0 && (strcmp(name, "readLine") == 0 ||
                           strcmp(name, "readInt") == 0 ||
                           strcmp(name, "readDouble") == 0 ||
                           strcmp(name, "readBoolean") == 0)) {
    const char *targetType = strcmp(name, "readLine") == 0
                                 ? "string"
                                 : (strcmp(name, "readInt") == 0
                                        ? "int"
                                        : (strcmp(name, "readDouble") == 0
                                               ? "float"
                                               : "bool"));
    *resultType = strcmp(name, "readLine") == 0
                      ? TYPE_STRING
                      : (strcmp(name, "readInt") == 0
                             ? TYPE_INT
                             : (strcmp(name, "readDouble") == 0
                                    ? TYPE_DOUBLE
                                    : TYPE_BOOLEAN));
    return Emit(codegen, "READ GF@_tmp0 %s\nPUSHS GF@_tmp0\n", targetType);
  }

  if (strcmp(name, "strLength") == 0 && args->count == 1) {
    if (!EmitExpression(codegen, args->items[0]) ||
        !Emit(codegen,
              "POPS GF@_tmp0\nSTRLEN GF@_tmp1 GF@_tmp0\nPUSHS GF@_tmp1\n")) {
      return false;
    }
    *resultType = TYPE_INT;
    return true;
  }

  if (strcmp(name, "toInt") == 0 && args->count == 1) {
    DataType_t argType = ExpressionType(codegen, args->items[0]);
    if (!EmitExpression(codegen, args->items[0])) {
      return false;
    }
    *resultType = TYPE_INT;
    if (argType == TYPE_INT) {
      return true;
    }
    return Emit(codegen,
                "POPS GF@_tmp0\nFLOAT2INT GF@_tmp1 GF@_tmp0\n"
                "PUSHS GF@_tmp1\n");
  }

  if (strcmp(name, "toStr") == 0 && args->count == 1) {
    DataType_t argType = ExpressionType(codegen, args->items[0]);
    if (!EmitExpression(codegen, args->items[0]) ||
        !Emit(codegen, "POPS GF@_tmp0\n")) {
      return false;
    }
    *resultType = TYPE_STRING;
    if (argType == TYPE_STRING) {
      return Emit(codegen, "PUSHS GF@_tmp0\n");
    }
    if (argType == TYPE_INT) {
      return Emit(codegen, "INT2STR GF@_tmp1 GF@_tmp0\nPUSHS GF@_tmp1\n");
    }
    if (argType == TYPE_DOUBLE) {
      return Emit(codegen,
                  "FLOAT2STR GF@_tmp1 GF@_tmp0\nPUSHS GF@_tmp1\n");
    }
    if (argType == TYPE_UNIT) {
      return Emit(codegen, "MOVE GF@_tmp1 string@()\nPUSHS GF@_tmp1\n");
    }
    if (argType == TYPE_BOOLEAN) {
      size_t trueLabel = codegen->nextLabel++;
      size_t endLabel = codegen->nextLabel++;
      return Emit(codegen,
                  "JUMPIFEQ $L%zu GF@_tmp0 bool@true\n"
                  "MOVE GF@_tmp1 string@false\n"
                  "JUMP $L%zu\n"
                  "LABEL $L%zu\n"
                  "MOVE GF@_tmp1 string@true\n"
                  "LABEL $L%zu\n"
                  "PUSHS GF@_tmp1\n",
                  trueLabel, endLabel, trueLabel, endLabel);
    }
    return false;
  }

  if (strcmp(name, "chr") == 0 && args->count == 1) {
    if (!EmitExpression(codegen, args->items[0]) ||
        !Emit(codegen,
              "POPS GF@_tmp0\nINT2CHAR GF@_tmp1 GF@_tmp0\n"
              "PUSHS GF@_tmp1\n")) {
      return false;
    }
    *resultType = TYPE_STRING;
    return true;
  }

  if (strcmp(name, "ord") == 0 && args->count == 2) {
    if (!EmitExpression(codegen, args->items[0]) ||
        !EmitExpression(codegen, args->items[1])) {
      return false;
    }
    size_t invalidLabel = codegen->nextLabel++;
    size_t endLabel = codegen->nextLabel++;
    *resultType = TYPE_INT;
    return Emit(codegen,
                "POPS GF@_tmp1\nPOPS GF@_tmp0\n"
                "STRLEN GF@_tmp2 GF@_tmp0\n"
                "LT GF@_tmp3 GF@_tmp1 int@0\n"
                "JUMPIFEQ $L%zu GF@_tmp3 bool@true\n"
                "LT GF@_tmp3 GF@_tmp1 GF@_tmp2\n"
                "JUMPIFEQ $L%zu GF@_tmp3 bool@false\n"
                "STRI2INT GF@_tmp3 GF@_tmp0 GF@_tmp1\n"
                "PUSHS GF@_tmp3\nJUMP $L%zu\n"
                "LABEL $L%zu\nPUSHS int@-1\n"
                "LABEL $L%zu\n",
                invalidLabel, invalidLabel, endLabel, invalidLabel, endLabel);
  }

  if (strcmp(name, "subStr") == 0 && args->count == 3) {
    if (!EmitExpression(codegen, args->items[0]) ||
        !EmitExpression(codegen, args->items[1]) ||
        !EmitExpression(codegen, args->items[2])) {
      return false;
    }
    size_t errorLabel = codegen->nextLabel++;
    size_t loopLabel = codegen->nextLabel++;
    size_t doneLabel = codegen->nextLabel++;
    size_t endLabel = codegen->nextLabel++;
    *resultType = TYPE_STRING;
    return Emit(codegen,
                "POPS GF@_tmp2\nPOPS GF@_tmp1\nPOPS GF@_tmp0\n"
                "STRLEN GF@_tmp3 GF@_tmp0\n"
                "LT GF@_tmp4 GF@_tmp1 int@0\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@true\n"
                "LT GF@_tmp4 GF@_tmp2 int@0\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@true\n"
                "GT GF@_tmp4 GF@_tmp1 GF@_tmp2\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@true\n"
                "GT GF@_tmp4 GF@_tmp1 GF@_tmp3\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@true\n"
                "GT GF@_tmp4 GF@_tmp2 GF@_tmp3\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@true\n"
                "MOVE GF@_tmp3 string@\n"
                "LABEL $L%zu\n"
                "LT GF@_tmp4 GF@_tmp1 GF@_tmp2\n"
                "JUMPIFEQ $L%zu GF@_tmp4 bool@false\n"
                "GETCHAR GF@_tmp4 GF@_tmp0 GF@_tmp1\n"
                "CONCAT GF@_tmp3 GF@_tmp3 GF@_tmp4\n"
                "ADD GF@_tmp1 GF@_tmp1 int@1\n"
                "JUMP $L%zu\n"
                "LABEL $L%zu\n"
                "PUSHS GF@_tmp3\nJUMP $L%zu\n"
                "LABEL $L%zu\nEXIT int@40\n"
                "LABEL $L%zu\n",
                errorLabel, errorLabel, errorLabel, errorLabel, errorLabel,
                loopLabel, doneLabel, loopLabel, doneLabel, endLabel,
                errorLabel, endLabel);
  }

  return false;
}

/* Emit a user-function call, or delegate to the built-in handler. */
static bool EmitCall(Codegen_t *codegen, const ASTNode_t *node,
                     DataType_t *resultType) {
  const char *name = node->fncall.name;
  if (name == NULL) {
    return false;
  }

  const FunctionInfo_t *function = FindFunction(codegen, name);
  if (function == NULL) {
    return EmitBuiltinCall(codegen, node, resultType);
  }

  const ASTList_t *args = &node->fncall.args;
  const ASTList_t *params = &function->node->fnDef.params;
  if (args->count != params->count ||
      (args->count != 0 && args->items == NULL) ||
      (params->count != 0 && params->items == NULL)) {
    return false;
  }
  for (size_t i = 0; i < args->count; i++) {
    const ASTNode_t *param = params->items[i];
    if (param == NULL || param->kind != NODE_PARAM ||
        !EmitExpression(codegen, args->items[i]) ||
        !ConvertStackValue(codegen, ExpressionType(codegen, args->items[i]),
                           param->param.declType)) {
      return false;
    }
  }

  if (!Emit(codegen, "CREATEFRAME\n")) {
    return false;
  }
  for (size_t i = 0; i < params->count; i++) {
    if (!Emit(codegen, "DEFVAR TF@arg%zu\n", i)) {
      return false;
    }
  }
  for (size_t i = params->count; i > 0; i--) {
    if (!Emit(codegen, "POPS TF@arg%zu\n", i - 1)) {
      return false;
    }
  }
  *resultType = function->node->fnDef.declType;
  return Emit(codegen, "CALL $fn%zu\n", function->id);
}

/* Emit code for a value-producing AST node, leaving its result on the stack. */
static bool EmitExpression(Codegen_t *codegen, const ASTNode_t *node) {
  if (node == NULL) {
    return false;
  }

  switch (node->kind) {
  case NODE_ID: {
    const Binding_t *binding = FindBinding(codegen, node->name);
    if (binding == NULL) {
      return false;
    }
    return Emit(codegen, "PUSHS LF@%s%zu\n",
                binding->isParameter ? "arg" : "v", binding->id);
  }
  case NODE_INT:
    return Emit(codegen, "PUSHS int@%d\n", node->intVal);
  case NODE_DOUBLE:
    return Emit(codegen, "PUSHS float@%a\n", node->dblVal);
  case NODE_STRING:
    return Emit(codegen, "PUSHS ") && EscapeString(codegen, node->string) &&
           Emit(codegen, "\n");
  case NODE_UNIT:
    return Emit(codegen, "PUSHS nil@nil\n");
  case NODE_CONV_DOUBLE:
    return ExpressionType(codegen, node->value) == TYPE_INT &&
           EmitExpression(codegen, node->value) &&
           Emit(codegen, "INT2FLOATS\n");
  case NODE_FNCALL: {
    DataType_t resultType = TYPE_UNKNOWN;
    return EmitCall(codegen, node, &resultType);
  }
  case NODE_UNOP: {
    DataType_t operandType = ExpressionType(codegen, node->unOp.operand);
    if (node->unOp.op == OP_UPLUS) {
      return EmitExpression(codegen, node->unOp.operand);
    }
    if (node->unOp.op == OP_NEG) {
      return EmitExpression(codegen, node->unOp.operand) &&
             Emit(codegen, "NOTS\n");
    }
    if (node->unOp.op != OP_UMINUS ||
        !EmitExpression(codegen, node->unOp.operand)) {
      return false;
    }
    return Emit(codegen, "POPS GF@_tmp0\nPUSHS %s@0\nPUSHS GF@_tmp0\nSUBS\n",
                operandType == TYPE_DOUBLE ? "float" : "int");
  }
  case NODE_BINOP:
  case NODE_COND: {
    const ASTNode_t *left = node->binOp.left;
    const ASTNode_t *right = node->binOp.right;
    Operator_t op = node->binOp.op;
    DataType_t leftType = ExpressionType(codegen, left);
    DataType_t rightType = ExpressionType(codegen, right);

    if (op == OP_AND || op == OP_OR) {
      size_t shortcutLabel = codegen->nextLabel++;
      size_t endLabel = codegen->nextLabel++;
      const char *shortcutValue = op == OP_AND ? "false" : "true";
      return EmitExpression(codegen, left) &&
             Emit(codegen,
                  "POPS GF@_tmp0\nJUMPIFEQ $L%zu GF@_tmp0 bool@%s\n",
                  shortcutLabel, shortcutValue) &&
             EmitExpression(codegen, right) &&
             Emit(codegen,
                  "JUMP $L%zu\nLABEL $L%zu\nPUSHS bool@%s\nLABEL $L%zu\n",
                  endLabel, shortcutLabel, shortcutValue, endLabel);
    }

    DataType_t commonType =
        leftType == TYPE_DOUBLE || rightType == TYPE_DOUBLE ? TYPE_DOUBLE
                                                             : leftType;

    if (op == OP_PLUS && (leftType == TYPE_STRING || rightType == TYPE_STRING)) {
      if (!EmitExpression(codegen, left) || !EmitExpression(codegen, right)) {
        return false;
      }
      return Emit(codegen,
                  "POPS GF@_tmp1\nPOPS GF@_tmp0\n"
                  "CONCAT GF@_tmp2 GF@_tmp0 GF@_tmp1\nPUSHS GF@_tmp2\n");
    }

    if (!EmitExpression(codegen, left) ||
        !ConvertStackValue(codegen, leftType, commonType) ||
        !EmitExpression(codegen, right) ||
        !ConvertStackValue(codegen, rightType, commonType)) {
      return false;
    }

    switch (op) {
    case OP_PLUS:
      return Emit(codegen, "ADDS\n");
    case OP_MINUS:
      return Emit(codegen, "SUBS\n");
    case OP_MUL:
      return Emit(codegen, "MULS\n");
    case OP_DIV:
      return Emit(codegen,
                  commonType == TYPE_INT ? "IDIVS\n" : "DIVS\n");
    case OP_EQ:
      return Emit(codegen, "EQS\n");
    case OP_NEQ:
      return Emit(codegen, "EQS\nNOTS\n");
    case OP_LT:
      return Emit(codegen, "LTS\n");
    case OP_GT:
      return Emit(codegen, "GTS\n");
    case OP_LTE:
      return Emit(codegen, "GTS\nNOTS\n");
    case OP_GTE:
      return Emit(codegen, "LTS\nNOTS\n");
    default:
      return false;
    }
  }
  case NODE_IFEXPR: {
    DataType_t resultType = ExpressionType(codegen, node);
    if (resultType == TYPE_UNKNOWN) {
      return false;
    }
    size_t elseLabel = codegen->nextLabel++;
    size_t endLabel = codegen->nextLabel++;
    return EmitExpression(codegen, node->ifexpr.cond) &&
           Emit(codegen,
                "POPS GF@_tmp0\nJUMPIFEQ $L%zu GF@_tmp0 bool@false\n",
                elseLabel) &&
           EmitExpression(codegen, node->ifexpr.thenExpr) &&
           ConvertStackValue(codegen,
                             ExpressionType(codegen, node->ifexpr.thenExpr),
                             resultType) &&
           Emit(codegen, "JUMP $L%zu\nLABEL $L%zu\n", endLabel, elseLabel) &&
           EmitExpression(codegen, node->ifexpr.elseExpr) &&
           ConvertStackValue(codegen,
                             ExpressionType(codegen, node->ifexpr.elseExpr),
                             resultType) &&
           Emit(codegen, "LABEL $L%zu\n", endLabel);
  }
  default:
    return false;
  }
}

/* Emit each statement in a block and then discard its local-name bindings. */
static bool EmitBlock(Codegen_t *codegen, const ASTNode_t *block) {
  if (block == NULL || block->kind != NODE_BLOCK ||
      (block->block.count != 0 && block->block.items == NULL)) {
    return false;
  }
  size_t scopeStart = codegen->bindingCount;
  for (size_t i = 0; i < block->block.count; i++) {
    if (!EmitStatement(codegen, block->block.items[i])) {
      codegen->bindingCount = scopeStart;
      return false;
    }
  }
  codegen->bindingCount = scopeStart;
  return true;
}

static bool EmitForLoop(Codegen_t *codegen, const ASTNode_t *node) {
  const ASTNode_t *generator = node->forLoop.gen;
  const LocalInfo_t *value =
      FindLocal(codegen->currentFunction, generator);
  const LoopInfo_t *loop = FindLoopInfo(codegen->currentFunction, node);
  if (generator == NULL || generator->kind != NODE_GEN || value == NULL ||
      loop == NULL || node->forLoop.body == NULL) {
    return false;
  }

  size_t bindingStart = codegen->bindingCount;
  size_t startLabel = codegen->nextLabel++;
  size_t negativeLabel = codegen->nextLabel++;
  size_t bodyLabel = codegen->nextLabel++;
  size_t endLabel = codegen->nextLabel++;
  size_t nonZeroStepLabel = codegen->nextLabel++;

  bool success = EmitExpression(codegen, generator->gen.from) &&
      Emit(codegen, "POPS LF@v%zu\n", value->id) &&
      EmitExpression(codegen, generator->gen.to) &&
      Emit(codegen, "POPS LF@v%zu\n", loop->endId);

  if (success) {
    if (generator->gen.by == NULL) {
      success = Emit(codegen, "MOVE LF@v%zu int@1\n", loop->stepId);
    } else {
      success = EmitExpression(codegen, generator->gen.by) &&
                Emit(codegen, "POPS LF@v%zu\n", loop->stepId);
    }
  }

  if (success) {
    success =
        AddBinding(codegen, generator->gen.name, TYPE_INT, value->id, false);
  }

  success = success &&
            Emit(codegen,
                 "LABEL $L%zu\n"
                 "JUMPIFNEQ $L%zu LF@v%zu int@0\n"
                 "EXIT int@10\n"
                 "LABEL $L%zu\n"
                 "LT GF@_tmp0 LF@v%zu int@0\n"
                 "JUMPIFEQ $L%zu GF@_tmp0 bool@true\n",
                 startLabel, nonZeroStepLabel, loop->stepId,
                 nonZeroStepLabel, loop->stepId, negativeLabel);

  if (success) {
    if (generator->gen.IncludeMax) {
      success = Emit(codegen,
                     "GT GF@_tmp0 LF@v%zu LF@v%zu\n"
                     "JUMPIFEQ $L%zu GF@_tmp0 bool@true\n",
                     value->id, loop->endId, endLabel);
    } else {
      success = Emit(codegen,
                     "LT GF@_tmp0 LF@v%zu LF@v%zu\n"
                     "JUMPIFEQ $L%zu GF@_tmp0 bool@false\n",
                     value->id, loop->endId, endLabel);
    }
  }

  success =
      success && Emit(codegen, "JUMP $L%zu\nLABEL $L%zu\n", bodyLabel,
                      negativeLabel);

  if (success) {
    if (generator->gen.IncludeMax) {
      success = Emit(codegen,
                     "LT GF@_tmp0 LF@v%zu LF@v%zu\n"
                     "JUMPIFEQ $L%zu GF@_tmp0 bool@true\n",
                     value->id, loop->endId, endLabel);
    } else {
      success = Emit(codegen,
                     "GT GF@_tmp0 LF@v%zu LF@v%zu\n"
                     "JUMPIFEQ $L%zu GF@_tmp0 bool@false\n",
                     value->id, loop->endId, endLabel);
    }
  }

  success =
      success && Emit(codegen, "LABEL $L%zu\n", bodyLabel) &&
      EmitBlock(codegen, node->forLoop.body) &&
      Emit(codegen,
           "ADD LF@v%zu LF@v%zu LF@v%zu\n"
           "JUMP $L%zu\n"
           "LABEL $L%zu\n",
           value->id, value->id, loop->stepId, startLabel, endLabel);
  codegen->bindingCount = bindingStart;
  return success;
}

/* Emit code for declarations, assignments, control flow, returns, or calls. */
static bool EmitStatement(Codegen_t *codegen, const ASTNode_t *node) {
  if (node == NULL) {
    return false;
  }
  switch (node->kind) {
  case NODE_VALDEF:
  case NODE_VARDEF: {
    const LocalInfo_t *local =
        FindLocal(codegen->currentFunction, node);
    if (local == NULL || !EmitExpression(codegen, node->varDef.init) ||
        !ConvertStackValue(codegen, ExpressionType(codegen, node->varDef.init),
                           local->type) ||
        !Emit(codegen, "POPS LF@v%zu\n", local->id)) {
      return false;
    }
    return AddBinding(codegen, node->varDef.name, local->type, local->id,
                      false);
  }
  case NODE_ASSIGN: {
    const Binding_t *binding = FindBinding(codegen, node->assign.name);
    return binding != NULL && !binding->isParameter &&
           EmitExpression(codegen, node->assign.value) &&
           ConvertStackValue(codegen, ExpressionType(codegen, node->assign.value),
                             binding->type) &&
           Emit(codegen, "POPS LF@v%zu\n", binding->id);
  }
  case NODE_IF: {
    size_t elseLabel = codegen->nextLabel++;
    size_t endLabel = codegen->nextLabel++;
    if (!EmitExpression(codegen, node->ifElse.cond) ||
        !Emit(codegen,
              "POPS GF@_tmp0\nJUMPIFEQ $L%zu GF@_tmp0 bool@false\n",
              elseLabel) ||
        !EmitBlock(codegen, node->ifElse.thenBlock) ||
        !Emit(codegen, "JUMP $L%zu\nLABEL $L%zu\n", endLabel, elseLabel)) {
      return false;
    }
    if (node->ifElse.elseBlock != NULL &&
        !EmitBlock(codegen, node->ifElse.elseBlock)) {
      return false;
    }
    return Emit(codegen, "LABEL $L%zu\n", endLabel);
  }
  case NODE_WHILE: {
    size_t startLabel = codegen->nextLabel++;
    size_t endLabel = codegen->nextLabel++;
    return Emit(codegen, "LABEL $L%zu\n", startLabel) &&
           EmitExpression(codegen, node->whileDo.cond) &&
           Emit(codegen,
                "POPS GF@_tmp0\nJUMPIFEQ $L%zu GF@_tmp0 bool@false\n",
                endLabel) &&
           EmitBlock(codegen, node->whileDo.body) &&
           Emit(codegen, "JUMP $L%zu\nLABEL $L%zu\n", startLabel, endLabel);
  }
  case NODE_FOR:
    return EmitForLoop(codegen, node);
  case NODE_RETURN:
    if (node->ret.value != NULL) {
      if (!EmitExpression(codegen, node->ret.value) ||
          !ConvertStackValue(codegen, ExpressionType(codegen, node->ret.value),
                             codegen->currentFunction->node->fnDef.declType)) {
        return false;
      }
    } else if (!Emit(codegen, "PUSHS nil@nil\n")) {
      return false;
    }
    return Emit(codegen, "POPFRAME\nRETURN\n");
  case NODE_BLOCK:
    return EmitBlock(codegen, node);
  default:
    if (!IsExpression(node) || !EmitExpression(codegen, node)) {
      return false;
    }
    return Emit(codegen, "POPS GF@_discard\n");
  }
}

/* Prepare a function's parameters and locals, then emit its body and return. */
static bool EmitFunction(Codegen_t *codegen, FunctionInfo_t *function) {
  codegen->currentFunction = function;
  codegen->bindingCount = 0;
  codegen->nextLocalId = 0;

  const ASTNode_t *node = function->node;
  if (node->fnDef.name == NULL || node->fnDef.body == NULL ||
      node->fnDef.body->kind != NODE_BLOCK ||
      (node->fnDef.params.count != 0 && node->fnDef.params.items == NULL)) {
    return false;
  }

  for (size_t i = 0; i < node->fnDef.params.count; i++) {
    const ASTNode_t *param = node->fnDef.params.items[i];
    if (param == NULL || param->kind != NODE_PARAM ||
        !AddBinding(codegen, param->param.name, param->param.declType, i,
                    true)) {
      return false;
    }
  }

  if (!CollectBlock(codegen, function, node->fnDef.body)) {
    return false;
  }

  if (!Emit(codegen, "LABEL $fn%zu\nPUSHFRAME\n", function->id)) {
    return false;
  }
  for (size_t i = 0; i < function->localCount; i++) {
    if (!Emit(codegen, "DEFVAR LF@v%zu\n", function->locals[i].id)) {
      return false;
    }
  }

  const ASTNode_t *body = node->fnDef.body;
  size_t scopeStart = codegen->bindingCount;
  for (size_t i = 0; i < body->block.count; i++) {
    const ASTNode_t *statement = body->block.items[i];
    bool last = i + 1 == body->block.count;
    if (last && IsExpression(statement)) {
      if (!EmitExpression(codegen, statement) ||
          !ConvertStackValue(codegen, ExpressionType(codegen, statement),
                             node->fnDef.declType)) {
        codegen->bindingCount = scopeStart;
        return false;
      }
    } else if (!EmitStatement(codegen, statement)) {
      codegen->bindingCount = scopeStart;
      return false;
    }
  }
  codegen->bindingCount = scopeStart;

  if (body->block.count == 0 ||
      body->block.items[body->block.count - 1]->kind != NODE_RETURN) {
    if (body->block.count == 0 ||
        !IsExpression(body->block.items[body->block.count - 1])) {
      if (!Emit(codegen, "PUSHS nil@nil\n")) {
        return false;
      }
    }
    if (!Emit(codegen, "POPFRAME\nRETURN\n")) {
      return false;
    }
  }
  return true;
}

/* Validate the program AST and write the complete IFJcode26 program. */
bool IFJCode26Generate(const ASTNode_t *program, FILE *output) {
  if (program == NULL || program->kind != NODE_PROGRAM || output == NULL ||
      program->program.count == 0 || program->program.items == NULL) {
    return false;
  }

  Codegen_t codegen = {.output = output};
  codegen.functions = calloc(program->program.count, sizeof *codegen.functions);
  if (codegen.functions == NULL) {
    return false;
  }

  size_t mainCount = 0;
  for (size_t i = 0; i < program->program.count; i++) {
    const ASTNode_t *function = program->program.items[i];
    if (function == NULL || function->kind != NODE_FNDEF ||
        function->fnDef.name == NULL) {
      codegen.failed = true;
      break;
    }
    for (size_t j = 0; j < codegen.functionCount; j++) {
      if (strcmp(codegen.functions[j].node->fnDef.name,
                 function->fnDef.name) == 0) {
        codegen.failed = true;
        break;
      }
    }
    if (codegen.failed) {
      break;
    }
    FunctionInfo_t *info = &codegen.functions[codegen.functionCount];
    info->node = function;
    info->id = codegen.functionCount++;
    if (function->fnDef.isMain) {
      if (function->fnDef.params.count != 0) {
        codegen.failed = true;
        break;
      }
      mainCount++;
    }
  }

  const FunctionInfo_t *mainFunction = NULL;
  for (size_t i = 0; i < codegen.functionCount; i++) {
    if (codegen.functions[i].node->fnDef.isMain) {
      mainFunction = &codegen.functions[i];
    }
  }
  if (codegen.failed || mainCount != 1 || mainFunction == NULL) {
    free(codegen.functions);
    return false;
  }

  bool success = Emit(&codegen,
                     ".IFJcode26\n"
                     "DEFVAR GF@_discard\n"
                     "DEFVAR GF@_tmp0\n"
                     "DEFVAR GF@_tmp1\n"
                     "DEFVAR GF@_tmp2\n"
                     "DEFVAR GF@_tmp3\n"
                     "DEFVAR GF@_tmp4\n"
                     "JUMP $entry\n");

  for (size_t i = 0; success && i < codegen.functionCount; i++) {
    success = EmitFunction(&codegen, &codegen.functions[i]);
  }

  if (success) {
    success = Emit(&codegen,
                   "LABEL $entry\nCREATEFRAME\nCALL $fn%zu\n"
                   "POPS GF@_discard\nEXIT int@0\n",
                   mainFunction->id);
  }

  for (size_t i = 0; i < codegen.functionCount; i++) {
    free(codegen.functions[i].locals);
    free(codegen.functions[i].loops);
  }
  free(codegen.functions);
  free(codegen.bindings);
  return success && !codegen.failed && !ferror(output);
}
