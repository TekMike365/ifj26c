/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "as_tree.h"
#include "logger.h"

/* creates new empty node
 * returns false on memory allocation failure, else true
 */
ASTNode_t *ASTCreate(ASTKind_t kind, SourceSpan_t pos) {
  ASTNode_t *node = (ASTNode_t *)calloc(1, sizeof(ASTNode_t));
  if (node == NULL)
    return node;

  switch (kind) {
  case NODE_PROGRAM:
  case NODE_BLOCK:
  case NODE_ASSIGN:
  case NODE_WHILE:
  case NODE_IF:
  case NODE_RETURN:
  case NODE_FOR:
  case NODE_GEN:
    node->type = TYPE_NA;
    break;
  default:
    node->type = TYPE_UNKNOWN;
  }

  node->kind = kind;
  node->pos = pos;
  return node;
}
/* appends node at the end of list
 * returns NULL on mem allocation failure
 */
bool ASTListAppend(ASTList_t *list, ASTNode_t *node) {
  if (list == NULL || node == NULL)
    return false;

  if (list->count >= list->capacity) {
    size_t newCap = (list->capacity == 0) ? 4 : list->capacity * 2;
    ASTNode_t **newItems = realloc(list->items, newCap * sizeof(ASTNode_t *));

    if (newItems == NULL)
      return false;

    list->items = newItems;
    list->capacity = newCap;
  }

  list->items[list->count++] = node;
  return true;
}

/* Frees block alloccated for the list */
static void ASTFreeList(ASTList_t *list) {
  if (list == NULL)
    return;
  for (size_t i = 0; i < list->count; i++)
    ASTFree(list->items[i]);
  free(list->items);
}

/* Frees block allocated for the tree */
void ASTFree(ASTNode_t *node) {
  if (node == NULL)
    return;

  switch (node->kind) {

  case NODE_ID:
    free(node->name);
    break;

  case NODE_STRING:
    free(node->string);
    break;

  case NODE_PROGRAM:
    ASTFreeList(&node->program);
    break;

  case NODE_BLOCK:
    ASTFreeList(&node->block);
    break;

  case NODE_FNDEF:
    free(node->fnDef.name);
    ASTFreeList(&node->fnDef.params);
    ASTFree(node->fnDef.body);
    break;

  case NODE_PARAM:
    free(node->param.name);
    break;

  case NODE_VALDEF:
  case NODE_VARDEF:
    free(node->varDef.name);
    ASTFree(node->varDef.init);
    break;

  case NODE_ASSIGN:
    free(node->assign.name);
    ASTFree(node->assign.value);
    break;

  case NODE_IF:
    ASTFree(node->ifElse.cond);
    ASTFree(node->ifElse.thenBlock);
    ASTFree(node->ifElse.elseBlock);
    break;

  case NODE_WHILE:
    ASTFree(node->whileDo.cond);
    ASTFree(node->whileDo.body);
    break;

  case NODE_RETURN:
    ASTFree(node->ret.value);
    break;

  case NODE_UNOP:
    ASTFree(node->unOp.operand);
    break;

  case NODE_BINOP:
    ASTFree(node->binOp.left);
    ASTFree(node->binOp.right);
    break;

  case NODE_FNCALL:
    free(node->name);
    ASTFreeList(&node->fncall.args);
    break;
  case NODE_FOR:
    ASTFree(node->forLoop.gen);
    ASTFree(node->forLoop.body);
    break;
  case NODE_GEN:
    ASTFree(node->gen.from);
    ASTFree(node->gen.by);
    ASTFree(node->gen.to);
    free(node->gen.name);
    break;
  case NODE_IFEXPR:
    ASTFree(node->ifexpr.cond);
    ASTFree(node->ifexpr.thenExpr);
    ASTFree(node->ifexpr.elseExpr);
    break;
  case NODE_COND:
    ASTFree(node->binOp.left);
    ASTFree(node->binOp.right);
  default:
    break;
  }
  free(node); /* free current node as well */
}

#ifdef DEBUG

static const char *DataTypeToStr(DataType_t type) {
  switch (type) {
  case TYPE_INT:
    return "Int";
  case TYPE_DOUBLE:
    return "Double";
  case TYPE_BOOLEAN:
    return "Boolean";
  case TYPE_UNIT:
    return "Unit";
  case TYPE_STRING:
    return "String";
  default:
    return "?";
  }
}

static const char *OpToChar(Operator_t op) {
  switch (op) {
  case OP_DIV:
    return "/";
  case OP_MUL:
    return "*";
  case OP_PLUS:
    return "+";
  case OP_MINUS:
    return "-";
  case OP_EQ:
    return "==";
  case OP_NEQ:
    return "!=";
  case OP_LT:
    return "<";
  case OP_GT:
    return ">";
  case OP_GTE:
    return ">=";
  case OP_LTE:
    return "<=";
  case OP_UPLUS:
    return "+";
  case OP_UMINUS:
    return "-";
  case OP_NEG:
    return "!";
  case OP_AND:
    return "&&";
  case OP_OR:
    return "||";
  default:
    return "<?>";
  }
}

static void ASTDumpNode(const ASTNode_t *node, const char *prefix, bool isLast);

static void ASTDumpList(const ASTList_t *list, const char *prefix) {
  if (list == NULL)
    return;

  for (size_t i = 0; i < list->count; i++) {
    ASTDumpNode(list->items[i], prefix, i + 1 == list->count);
  }
}

static void ASTDumpNode(const ASTNode_t *node, const char *prefix,
                        bool isLast) {
  if (node == NULL)
    return;

#define LOG_NODE(fmt, ...)                                                     \
  DLOG_INFO("%s%s" fmt, prefix, isLast ? "└── " : "├── ", __VA_ARGS__)

  char nextPrefix[512];
  snprintf(nextPrefix, sizeof(nextPrefix), "%s%s", prefix,
           isLast ? "    " : "│   ");

  switch (node->kind) {
  case NODE_PROGRAM:
    LOG_NODE("PROGRAM (line %ld)", node->pos.line);
    ASTDumpList(&node->program, nextPrefix);
    break;

  case NODE_BLOCK:
    LOG_NODE("BLOCK (line %ld)", node->pos.line);
    ASTDumpList(&node->block, nextPrefix);
    break;

  case NODE_FNDEF:
    LOG_NODE("FNDEF \"%s\"%s : %s (line %ld)",
             node->fnDef.name ? node->fnDef.name : "<null>",
             node->fnDef.isMain ? " [@main]" : "",
             DataTypeToStr(node->fnDef.declType), node->pos.line);
    ASTDumpList(&node->fnDef.params, nextPrefix);
    ASTDumpNode(node->fnDef.body, nextPrefix, true);
    break;

  case NODE_PARAM:
    LOG_NODE("PARAM \"%s\" : %s",
             node->param.name ? node->param.name : "<null>",
             DataTypeToStr(node->param.declType));
    break;

  case NODE_VALDEF:
  case NODE_VARDEF:
    LOG_NODE("%s \"%s\" : %s (line %ld)",
             node->kind == NODE_VALDEF ? "VALDEF" : "VARDEF",
             node->varDef.name ? node->varDef.name : "<null>",
             DataTypeToStr(node->varDef.declType), node->pos.line);
    ASTDumpNode(node->varDef.init, nextPrefix, true);
    break;

  case NODE_ASSIGN:
    LOG_NODE("ASSIGN \"%s\" (line %ld)",
             node->assign.name ? node->assign.name : "<null>", node->pos.line);
    ASTDumpNode(node->assign.value, nextPrefix, true);
    break;

  case NODE_IF:
    LOG_NODE("IF (line %ld)", node->pos.line);
    ASTDumpNode(node->ifElse.cond, nextPrefix, false);
    ASTDumpNode(node->ifElse.thenBlock, nextPrefix,
                node->ifElse.elseBlock == NULL);
    ASTDumpNode(node->ifElse.elseBlock, nextPrefix, true);
    break;

  case NODE_WHILE:
    LOG_NODE("WHILE (line %ld)", node->pos.line);
    ASTDumpNode(node->whileDo.cond, nextPrefix, false);
    ASTDumpNode(node->whileDo.body, nextPrefix, true);
    break;

  case NODE_FOR:
    LOG_NODE("FOR \"%s\" (line %ld)",
             (node->forLoop.gen && node->forLoop.gen->gen.name)
                 ? node->forLoop.gen->gen.name
                 : "<null>",
             node->pos.line);
    ASTDumpNode(node->forLoop.gen, nextPrefix, false);
    ASTDumpNode(node->forLoop.body, nextPrefix, true);
    break;

  case NODE_GEN:
    LOG_NODE("GEN (%s)", node->gen.IncludeMax ? "to" : "until");
    ASTDumpNode(node->gen.from, nextPrefix, false);
    ASTDumpNode(node->gen.to, nextPrefix, node->gen.by == NULL);
    ASTDumpNode(node->gen.by, nextPrefix, true);
    break;

  case NODE_IFEXPR:
    LOG_NODE("IF (line %ld)", node->pos.line);
    ASTDumpNode(node->ifexpr.cond, nextPrefix, false);
    ASTDumpNode(node->ifexpr.thenExpr, nextPrefix, false);
    ASTDumpNode(node->ifexpr.elseExpr, nextPrefix, true);
    break;
  case NODE_RETURN:
    LOG_NODE("RETURN (line %ld)", node->pos.line);
    ASTDumpNode(node->ret.value, nextPrefix, true);
    break;

  case NODE_UNOP:
    LOG_NODE("UNOP (op= %s) [type: %s]", OpToChar(node->unOp.op),
             DataTypeToStr(node->type));
    ASTDumpNode(node->unOp.operand, nextPrefix, true);
    break;

  case NODE_BINOP:
    LOG_NODE("BINOP (op= %s) [type: %s]", OpToChar(node->binOp.op),
             DataTypeToStr(node->type));
    ASTDumpNode(node->binOp.left, nextPrefix, false);
    ASTDumpNode(node->binOp.right, nextPrefix, true);
    break;
  case NODE_COND:
    LOG_NODE("COND (op= %s) [type: %s]", OpToChar(node->binOp.op),
             DataTypeToStr(node->type));
    ASTDumpNode(node->binOp.left, nextPrefix, false);
    ASTDumpNode(node->binOp.right, nextPrefix, true);
    break;
  case NODE_FNCALL:
    LOG_NODE("FNCALL \"%s\" [type: %s]",
             node->fncall.name ? node->fncall.name : "<null>",
             DataTypeToStr(node->type));
    ASTDumpList(&node->fncall.args, nextPrefix);
    break;

  case NODE_ID:
    LOG_NODE("ID \"%s\" [type: %s]", node->name ? node->name : "<null>",
             DataTypeToStr(node->type));
    break;

  case NODE_INT:
    LOG_NODE("INT %lld [type: %s]", node->intVal, DataTypeToStr(node->type));
    break;

  case NODE_DOUBLE:
    LOG_NODE("DOUBLE %g [type: %s]", node->dblVal, DataTypeToStr(node->type));
    break;

  case NODE_STRING:
    LOG_NODE("STRING \"%s\" [type: %s]", node->string ? node->string : "",
             DataTypeToStr(node->type));
    break;

  case NODE_UNIT:
    LOG_NODE("UNIT [type: %s]", DataTypeToStr(node->type));
    break;

  default:
    LOG_NODE("UNKNOWN_NODE (kind=%d)", node->kind);
    break;
  }
#undef LOG_NODE
}

void ASTDump(const ASTNode_t *root) {
  if (root == NULL) {
    DLOG_INFO("<empty AST>");
    return;
  }
  ASTDumpNode(root, "", true);
}

#else /* ifdef DEBUG */

void ASTDump(const ASTNode_t *root) { (void)root; }

#endif /* ifdef DEBUG */
