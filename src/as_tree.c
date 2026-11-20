/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "as_tree.h"

/* creates new empty node
 * returns false on memory allocation failure, else true
 */
ASTNode_t *ASTCreate(ASTKind_t kind, SourceSpan_t pos) {
  ASTNode_t *node = (ASTNode_t *)calloc(1, sizeof(ASTNode_t));
  if (node == NULL)
    return node;

  node->kind = kind;
  node->pos = pos;
  return node;
}
/* appends node at the end of list
 * returns false on mem allocation failure, else true
 */
bool ASTAppend(ASTList_t *list, ASTNode_t *node) {
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

  default:
    break;
  }
  free(node); /* free current node as well */
}

/* for DEBUG */
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

static char BinOpChar(Operator_t op) {
  switch (op) {
  case OP_DIV:
    return '/';
  case OP_MUL:
    return '*';
  case OP_PLUS:
    return '+';
  case OP_MINUS:
    return '-';
  case OP_EQ:
    return '=';
  case OP_NEQ:
    return '!';
  case OP_LT:
    return '<';
  case OP_GT:
    return '>';
  case OP_GTE:
    return 'G';
  case OP_LTE:
    return 'L';
  default:
    return '?';
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

// for DEBUG
static void ASTDumpNode(const ASTNode_t *node, const char *prefix,
                        bool isLast) {
  if (node == NULL)
    return;

  printf("%s%s", prefix, isLast ? "└── " : "├── ");

  char nextPrefix[512];
  snprintf(nextPrefix, sizeof(nextPrefix), "%s%s", prefix,
           isLast ? "    " : "│   ");

  switch (node->kind) {
  case NODE_PROGRAM:
    printf("PROGRAM (line %ld)\n", node->pos.line);
    ASTDumpList(&node->program, nextPrefix);
    break;

  case NODE_BLOCK:
    printf("BLOCK (line %ld)\n", node->pos.line);
    ASTDumpList(&node->block, nextPrefix);
    break;

  case NODE_FNDEF:
    printf("FNDEF \"%s\"%s : %s (line %ld)\n",
           node->fnDef.name ? node->fnDef.name : "<null>",
           node->fnDef.isMain ? " [@main]" : "",
           DataTypeToStr(node->fnDef.declType), node->pos.line);
    ASTDumpList(&node->fnDef.params, nextPrefix);
    ASTDumpNode(node->fnDef.body, nextPrefix, true);
    break;

  case NODE_PARAM:
    printf("PARAM \"%s\" : %s\n",
           node->param.name ? node->param.name : "<null>",
           DataTypeToStr(node->param.declType));
    break;

  case NODE_VALDEF:
  case NODE_VARDEF:
    printf("%s \"%s\" : %s (line %ld)\n",
           node->kind == NODE_VALDEF ? "VALDEF" : "VARDEF",
           node->varDef.name ? node->varDef.name : "<null>",
           DataTypeToStr(node->varDef.declType), node->pos.line);
    ASTDumpNode(node->varDef.init, nextPrefix, true);
    break;

  case NODE_ASSIGN:
    printf("ASSIGN \"%s\" (line %ld)\n",
           node->assign.name ? node->assign.name : "<null>", node->pos.line);
    ASTDumpNode(node->assign.value, nextPrefix, true);
    break;

  case NODE_IF:
    printf("IF (line %ld)\n", node->pos.line);
    ASTDumpNode(node->ifElse.cond, nextPrefix, false);
    ASTDumpNode(node->ifElse.thenBlock, nextPrefix,
                node->ifElse.elseBlock == NULL);
    ASTDumpNode(node->ifElse.elseBlock, nextPrefix, true);
    break;

  case NODE_WHILE:
    printf("WHILE (line %ld)\n", node->pos.line);
    ASTDumpNode(node->whileDo.cond, nextPrefix, false);
    ASTDumpNode(node->whileDo.body, nextPrefix, true);
    break;

  case NODE_RETURN:
    printf("RETURN (line %ld)\n", node->pos.line);
    ASTDumpNode(node->ret.value, nextPrefix, true);
    break;

  case NODE_UNOP:
    printf("UNOP (op= %c) [type: %s]\n",
           (node->unOp.op == OP_UMINUS) ? '-' : '+', DataTypeToStr(node->type));
    ASTDumpNode(node->unOp.operand, nextPrefix, true);
    break;

  case NODE_BINOP:
    printf("BINOP (op= %c) [type: %s]\n", BinOpChar(node->binOp.op),
           DataTypeToStr(node->type));
    ASTDumpNode(node->binOp.left, nextPrefix, false);
    ASTDumpNode(node->binOp.right, nextPrefix, true);
    break;

  case NODE_FNCALL:
    printf("FNCALL \"%s\" [type: %s]\n",
           node->fncall.name ? node->fncall.name : "<null>",
           DataTypeToStr(node->type));
    ASTDumpList(&node->fncall.args, nextPrefix);
    break;

  case NODE_ID:
    printf("ID \"%s\" [type: %s]\n", node->name ? node->name : "<null>",
           DataTypeToStr(node->type));
    break;

  case NODE_INT:
    printf("INT %lld [type: %s]\n", (long long)node->intVal,
           DataTypeToStr(node->type));
    break;

  case NODE_DOUBLE:
    printf("DOUBLE %g [type: %s]\n", node->dblVal, DataTypeToStr(node->type));
    break;

  case NODE_STRING:
    printf("STRING \"%s\" [type: %s]\n", node->string ? node->string : "",
           DataTypeToStr(node->type));
    break;

  case NODE_UNIT:
    printf("UNIT [type: %s]\n", DataTypeToStr(node->type));
    break;

  default:
    printf("UNKNOWN_NODE (kind=%d)\n", node->kind);
    break;
  }
}

void ASTDump(const ASTNode_t *root) {
  if (root == NULL) {
    printf("<empty AST>\n");
    return;
  }
  ASTDumpNode(root, "", true);
}

/* DEBUG */