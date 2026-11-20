/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "error.h"
#include "parser_internal.h"
#include "psa_stack.h"
#include "psa_table.h"

#define MAX_RULE_LENGTH 4

/* Returns AST node containing data from tokens: id, string, int, double, if
 * malloc failed returns NULL and sets error.
 * */
static ASTNode_t *CreateLeaf(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_ID, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_INTERNAL;
    return NULL;
  }
  /* get name from token */
  switch (p->current.type) {
  case TOKEN_IDENTIFIER:
    if (p->current.lexemeS == NULL)
      break;
    node->kind = NODE_ID;
    node->string = StrDup(p->current.lexemeS);
    return node;

  /* get string from token */
  case TOKEN_STRING:
    if (p->current.lexemeS == NULL)
      break;
    node->kind = NODE_STRING;
    node->type = TYPE_STRING;
    node->string = StrDup(p->current.lexemeS);
    return node;

  /* get int value from token, dont mind the zero */
  case TOKEN_ZERO:
  case TOKEN_INT:
    node->kind = NODE_INT;
    node->type = TYPE_INT;
    node->intVal = p->current.lexemeI;
    return node;

  /* get double value from token */
  case TOKEN_DOUBLE:
    node->kind = NODE_DOUBLE;
    node->type = TYPE_DOUBLE;
    node->dblVal = p->current.lexemeD;
    return node;

  default:
    break;
  }
  ASTFree(node);
  SyntaxError(p, "Invalid expression");
  return NULL;
}

// Takes ownership of the node from item, returns the node.
static ASTNode_t *TakeNode(PStackItem_t *item) {
  ASTNode_t *node = item->node;
  item->node = NULL;
  return node;
}

/* Appends new node to list (children of parent node)
 * If unable to append, sets ERROR_INTERNAL and returns false.
 */
static bool AppendNode(Parser_t *p, ASTList_t *list, PStackItem_t *item) {
  if (!ASTAppend(list, item->node)) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
    return false;
  }
  item->node = NULL;
  return true;
}

/* If unable to create, sets ERROR_INTERNAL and returns NULL. */
static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind, PStackItem_t *item) {
  SourceSpan_t pos =
      (item != NULL && item->node != NULL) ? item->node->pos : p->current.pos;

  ASTNode_t *node = ASTCreate(kind, pos);

  if (node == NULL) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
  }
  return node;
}

/* Reduces items on stack based on rule
 * If none of the reduction rules apply, sets ERROR_SYNTAX.
 * If fail on mem alloc, sets ERROR_INTERNAL.
 * retrns true of success, else false.
 */
static bool Reduce(Parser_t *p, PStack_t *stack) {
  PStackItem_t *items[MAX_RULE_LENGTH];
  PStackItem_t *current = stack->top;
  int len = 0;

  /* load items from top of stack up until handle */
  while (len < MAX_RULE_LENGTH && current != NULL &&
         current->sym != PT_HANDLE) {
    items[len] = current;
    current = current->next;
    len++;
  }

  /* handle not found */
  if (current == NULL || current->sym != PT_HANDLE)
    goto syntaxErr;

  ASTNode_t *newNode = NULL;
  PTSymbol_t newSym = PT_E;

  /* execute reduction rule */
  switch (len) {
  case 1:
    /* E -> id  */
    /* E -> val */
    if (items[0]->sym != PT_ID && items[0]->sym != PT_VAL)
      goto syntaxErr;
    newNode = TakeNode(items[0]);
    break;

  case 2:
    /* E -> ( )  */
    if (items[1]->sym == PT_LPAR && items[0]->sym == PT_RPAR) {
      if (!(newNode = NewNode(p, NODE_UNIT, items[1])))
        return false;

      newNode->type = TYPE_UNIT;
      break;
    }
    /* E -> + E */
    /* E -> - E */
    if (items[0]->sym != PT_E ||
        (items[1]->sym != PT_UMINUS && items[1]->sym != PT_UPLUS))
      goto syntaxErr;

    if (!(newNode = NewNode(p, NODE_UNOP, items[0])))
      return false;
    newNode->unOp.operand = TakeNode(items[0]);
    newNode->unOp.op = (items[1]->sym == PT_UMINUS) ? OP_UMINUS : OP_UPLUS;
    break;

  case 3:
    switch (items[2]->sym) {
    case PT_LPAR:
      /* E -> ( E ) */
      if (items[1]->sym != PT_E || items[0]->sym != PT_RPAR)
        goto syntaxErr;

      newNode = TakeNode(items[1]);
      break;

    case PT_ID: {
      /* E -> id ( ) */
      if (items[1]->sym != PT_LPAR || items[0]->sym != PT_RPAR ||
          items[2]->node->kind != NODE_ID)
        goto syntaxErr;
      /* changes node ID to node FNCALL */
      newNode = TakeNode(items[2]);
      char *name = newNode->name;
      newNode->kind = NODE_FNCALL;
      newNode->fncall.name = name;
      newNode->fncall.args = (ASTList_t){0};
      break;
    }

    case PT_ARGS:
      /* ARGS -> ARGS , E */
      if (items[1]->sym != PT_COMMA || items[0]->sym != PT_E)
        goto syntaxErr;
      /* takes helper node FNCALL, and appends E in the args list */
      newNode = TakeNode(items[2]);
      if (!AppendNode(p, &newNode->fncall.args, items[0])) {
        ASTFree(newNode);
        return false;
      }
      newSym = PT_ARGS;
      break;

    case PT_E:
      switch (items[1]->sym) {
      case PT_COMMA:
        /* ARGS -> E , E */
        if (items[0]->sym != PT_E)
          goto syntaxErr;
        /* Creates helper node FNCALL to then append E, E in the args list */
        if (!(newNode = NewNode(p, NODE_FNCALL, items[2])))
          return false;
        if (!AppendNode(p, &newNode->fncall.args, items[2])) {
          ASTFree(newNode);
          return false;
        }
        if (!AppendNode(p, &newNode->fncall.args, items[0])) {
          ASTFree(newNode);
          return false;
        }
        newSym = PT_ARGS;
        break;
      case PT_PLUS:
      case PT_MINUS:
      case PT_DIV:
      case PT_MUL:
      case PT_NEQ:
      case PT_EQ:
      case PT_GTE:
      case PT_GT:
      case PT_LTE:
      case PT_LT:
        /* E -> E op E  */
        /* COND -> E op E,  i think we dont need it explicitely */
        if (items[0]->sym != PT_E)
          goto syntaxErr;
        if (!(newNode = NewNode(p, NODE_BINOP, items[2])))
          return false;
        newNode->binOp.left = TakeNode(items[2]);
        newNode->binOp.right = TakeNode(items[0]);
        newNode->binOp.op = OpFromSym(items[1]->sym);
        break;

      default:
        goto syntaxErr;
      }
      break;

    default:
      goto syntaxErr;
    }
    break;

  case 4:
    if (items[3]->sym != PT_ID || items[2]->sym != PT_LPAR ||
        items[0]->sym != PT_RPAR || items[3]->node->kind != NODE_ID)
      goto syntaxErr;

    /* E -> id ( E ) */
    if (items[1]->sym == PT_E) {
      /* changes node ID to node FNCALL and appends E in args list */
      newNode = TakeNode(items[3]);
      char *name = newNode->name;
      newNode->kind = NODE_FNCALL;
      newNode->fncall.name = name;
      newNode->fncall.args = (ASTList_t){0};
      if (!AppendNode(p, &newNode->fncall.args, items[1])) {
        ASTFree(newNode);
        return false;
      }
    }

    /* E -> id ( ARGS ) */
    else if (items[1]->sym == PT_ARGS) {
      /* takes helper node FNCALL whi ch already stores the arguments and
       * sets the FNCALL name to the name from node ID, then frees node ID */
      newNode = TakeNode(items[1]);
      newNode->fncall.name = items[3]->node->name;
      items[3]->node->name = NULL;
      newNode->pos = items[3]->node->pos;
      ASTFree(TakeNode(items[3]));
    } else
      goto syntaxErr;
    break;

  default:
    goto syntaxErr;
  }

  /* set the new stack top after the handle */
  stack->top = current->next;
  /* free node handle */
  free(current);
  /* free popped nodes from stack */
  for (int i = 0; i < len; i++) {
    free(items[i]);
  }
  /* push the new reduced item to stack */
  if (!PStackPush(stack, newSym, newNode)) {
    ReportInternalError("Out of memory.");
    p->errorCode = ERROR_INTERNAL;
    ASTFree(newNode);
    return false;
  }
  return true;

syntaxErr:
  SyntaxError(p, "Invalid expression");
  return false;
}

/* Parses nonterminal expression using precedence parsing.
 * returns AS tree of the expression if successful, else NULL and sets error
 */
ASTNode_t *ParseExpression(Parser_t *p) {
  PStack_t stack;

  if (!PStackInit(&stack))
    goto stackError;

  /* repeat while not finished */
  while (true) {
    if (p->current.type == TOKEN_ERROR) {
      if (p->errorCode == ERROR_NOERR)
        p->errorCode = p->current.error;

      goto stackError;
    }

    PTSymbol_t topTerminal = PStackTopTerminal(&stack)->sym;
    PTSymbol_t currentSymbol = SymFromToken(p->current.type);

    if (topTerminal == PT_DOLLAR && currentSymbol == PT_DOLLAR) {

      if (stack.top->sym != PT_E || stack.top->next == NULL ||
          stack.top->next->sym != PT_DOLLAR)
        goto syntaxError;

      /* on success, stack should contain only: $ E  */
      ASTNode_t *expr = TakeNode(stack.top);
      PStackFree(&stack);
      return expr;
    }
    /* terminal priority in precedence table */
    switch (PTable[topTerminal][currentSymbol]) {

    case '<': {
      if (!PStackInsertHandle(&stack))
        goto stackError;
    }
    /* because we want to push token as well on case '<' */
    /* FALLTHROUGH */
    case '=': {
      /* push token on stack */
      ASTNode_t *node = NULL;

      /* if the token contains data, create Leaf Node */
      if (currentSymbol == PT_ID || currentSymbol == PT_VAL)
        if (!(node = CreateLeaf(p)))
          goto cleanup;

      if (!PStackPush(&stack, currentSymbol, node)) {
        ASTFree(node);
        goto stackError;
      }

      if (!Advance(p))
        goto cleanup;

      break;
    }

    case '>':
      /* search for reduction rule */
      if (!Reduce(p, &stack))
        goto cleanup;

      break;

    default:
      goto syntaxError;
    }
  }

syntaxError:
  if (p->errorCode == ERROR_NOERR) {
    SyntaxError(p, "Invalid expression");
    goto cleanup;
  }

stackError:
  ReportInternalError("Out of memory.");
  p->errorCode = ERROR_INTERNAL;

cleanup:
  PStackFree(&stack);
  return NULL;
}