/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */
#include "parser_internal.h"

#define MAX_RULE_LENGTH 4

typedef enum {
  // Terminals:

  PSA_ID = 0, // id, int, double, string
  PSA_COMMA,  // ,
  PSA_UPLUS,  // unary +
  PSA_UMINUS, // unary -
  PSA_LPAR,   // (
  PSA_RPAR,   // )
  PSA_MUL,    // *
  PSA_DIV,    // /
  PSA_PLUS,   // binary +
  PSA_MINUS,  // binary -
  PSA_LT,     // <
  PSA_LTE,    // <=
  PSA_GT,     // >
  PSA_GTE,    // >=
  PSA_EQ,     // ==
  PSA_NEQ,    // !=
  PSA_DOLLAR, // $

  // Nonterminals:

  PSA_HANDLE,
  PSA_E,
  PSA_ARGS,
} PSASymbol_t;

// made by AI and asked me to change PTable[1][1] to '>'
static const char PTable[17][17] = {
    {' ', '>', ' ', ' ', '=', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '<', '<', '<', '<', '=', '<', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', ' '},
    {' ', '>', ' ', ' ', ' ', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '>', '>', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', '>', '<', '<', '<', '>', '<', '<', '>', '>', '>', '>', '>', '>', '>',
     '>', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', ' ', '<', '<', '<', '>', '<', '<', '<', '<', ' ', ' ', ' ', ' ', ' ',
     ' ', '>'},
    {'<', '<', '<', '<', '<', ' ', '<', '<', '<', '<', '<', '<', '<', '<', '<',
     '<', ' '}};

static PSASymbol_t SymFromToken(TokenType_t t) {
  switch (t) {
  case TOKEN_IDENTIFIER:
    return PSA_ID;
  case TOKEN_INT:
    return PSA_ID;
  case TOKEN_DOUBLE:
    return PSA_ID;
  case TOKEN_STRING:
    return PSA_ID;
  case TOKEN_COMMA:
    return PSA_COMMA;
  case TOKEN_LPAR:
    return PSA_LPAR;
  case TOKEN_RPAR:
    return PSA_RPAR;
  case TOKEN_STAR:
    return PSA_MUL;
  case TOKEN_SLASH:
    return PSA_DIV;
  case TOKEN_PLUS:
    return PSA_PLUS;
  case TOKEN_UPLUS:
    return PSA_UPLUS;
  case TOKEN_MINUS:
    return PSA_MINUS;
  case TOKEN_UMINUS:
    return PSA_UMINUS;
  case TOKEN_LESS:
    return PSA_LT;
  case TOKEN_LESS_EQ:
    return PSA_LTE;
  case TOKEN_GREATER:
    return PSA_GT;
  case TOKEN_GREATER_EQ:
    return PSA_GTE;
  case TOKEN_DOUBLE_EQ:
    return PSA_EQ;
  case TOKEN_NOT_EQ:
    return PSA_NEQ;
  default:
    return PSA_DOLLAR;
  }
}

static Operator_t OpFromSym(PSASymbol_t sym) {
  switch (sym) {
  case PSA_EQ:
    return OP_EQ;
  case PSA_NEQ:
    return OP_NEQ;
  case PSA_LT:
    return OP_LT;
  case PSA_LTE:
    return OP_LTE;
  case PSA_GT:
    return OP_GT;
  case PSA_GTE:
    return OP_GTE;
  case PSA_PLUS:
    return OP_PLUS;
  case PSA_MINUS:
    return OP_MINUS;
  case PSA_DIV:
    return OP_DIV;
  case PSA_MUL:
    return OP_MUL;
  default:
    return OP_ERR;
  }
}

// Returns AST node containing data from tokens: id, string, int, double, if
// malloc failed returns NULL and sets error.
static ASTNode_t *CreateLeaf(Parser_t *p) {
  ASTNode_t *node = ASTCreate(NODE_ID, p->current.pos);
  if (node == NULL) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  switch (p->current.type) {
  case TOKEN_IDENTIFIER:
    if (p->current.lexemeS == NULL)
      break;
    node->kind = NODE_ID;
    node->string = p->current.lexemeS;
    p->current.lexemeS = NULL;
    return node;

  case TOKEN_STRING:
    if (p->current.lexemeS == NULL)
      break;
    node->kind = NODE_STRING;
    node->string = p->current.lexemeS;
    p->current.lexemeS = NULL;
    return node;

  case TOKEN_INT:
    node->kind = NODE_INT;
    node->intVal = p->current.lexemeI;
    return node;

  case TOKEN_DOUBLE:
    node->kind = NODE_DOUBLE;
    node->dblVal = p->current.lexemeD;
    return node;

  default:
    break;
  }
  ASTFree(node);
  SetError(p);
  return NULL;
}

// conatins symbol used for PSA
// if terminal or nonterminal, the node points to AS tree, else NULL
typedef struct PStackItem_t {
  ASTNode_t *node;
  PSASymbol_t sym;
  struct PStackItem_t *next;
} PStackItem_t;

// contains terminals nonterminals or handles plus AS trees.
typedef struct PStack_t {
  PStackItem_t *top;
} PStack_t;

static bool StackInit(PStack_t *stack) {
  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));

  if (newItem == NULL)
    return false;

  newItem->sym = PSA_DOLLAR;
  newItem->node = NULL;
  newItem->next = NULL;
  stack->top = newItem;

  return true;
}

static void StackFree(PStack_t *stack) {
  PStackItem_t *current = stack->top;
  while (current != NULL) {
    PStackItem_t *next = current->next;
    ASTFree(current->node);
    free(current);
    current = next;
  }
  stack->top = NULL;
}

// inserts handle after top-most terminal
static bool StackInsertHandle(PStack_t *stack) {
  if (stack == NULL || stack->top == NULL)
    return false;

  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));
  if (newItem == NULL)
    return false;

  newItem->sym = PSA_HANDLE;
  newItem->node = NULL;
  // terminal is on top
  if (stack->top->sym <= PSA_DOLLAR) {
    newItem->next = stack->top;
    stack->top = newItem;
    return true;
  }
  // terminal is deeper in stack
  PStackItem_t *current = stack->top;
  while (current->next != NULL && current->next->sym > PSA_DOLLAR) {
    current = current->next;
  }

  if (current->next == NULL) {
    free(newItem);
    return false;
  }

  newItem->next = current->next;
  current->next = newItem;
  return true;
}

// If fails on memory allocation returns false, else true.
static bool StackPush(PStack_t *stack, PSASymbol_t sym, ASTNode_t *node) {
  if (stack == NULL)
    return false;

  PStackItem_t *newItem = (PStackItem_t *)malloc(sizeof(PStackItem_t));
  if (newItem == NULL)
    return false;

  newItem->sym = sym;
  newItem->next = stack->top;
  newItem->node = node;
  stack->top = newItem;

  return true;
}

// returns the top-most terminal on the stack, return NULL if no terminal found
static PStackItem_t *StackTopTerminal(const PStack_t *stack) {
  if (stack == NULL)
    return NULL;

  PStackItem_t *current = stack->top;
  while (current != NULL && current->sym > PSA_DOLLAR) {
    current = current->next;
  }
  return current;
}

// Takes ownership of AST node from item, returns the node.
static ASTNode_t *TakeNode(PStackItem_t *item) {
  ASTNode_t *node = item->node;
  item->node = NULL;
  return node;
}

// If unable to append, sets ERROR_MEM in p->errorCode and returns false.
static bool AppendNode(Parser_t *p, ASTList_t *list, PStackItem_t *item) {
  if (!ASTAppend(list, item->node)) {
    p->errorCode = ERROR_MEM;
    return false;
  }
  item->node = NULL;
  return true;
}

// If unable to create, sets ERROR_MEM in p->errorCode and returns NULL.
static ASTNode_t *NewNode(Parser_t *p, ASTKind_t kind, PStackItem_t *item) {
  SourceSpan_t pos =
      (item != NULL && item->node != NULL) ? item->node->pos : p->current.pos;

  ASTNode_t *node = ASTCreate(kind, pos);

  if (node == NULL)
    p->errorCode = ERROR_MEM;

  return node;
}

// Reduces items on stack based on rule
// If none of the reduction rules apply, sets ERROR_SYNTAX in p->errorCode.
// If fails to allocate memory, sets ERROR_MEM in p->errorCode.
// If successful returns true, else false.
static bool Reduce(Parser_t *p, PStack_t *stack) {
  PStackItem_t *items[MAX_RULE_LENGTH];
  PStackItem_t *current = stack->top;
  int len = 0;

  // load items from top of stack up until handle
  while (len < MAX_RULE_LENGTH && current != NULL &&
         current->sym != PSA_HANDLE) {
    items[len] = current;
    current = current->next;
    len++;
  }

  // handle not found
  if (current == NULL || current->sym != PSA_HANDLE)
    goto syntaxErr;

  ASTNode_t *newNode = NULL;
  PSASymbol_t newSym = PSA_E;

  // execute reduction rule
  switch (len) {
  case 1:
    // E -> id
    if (items[0]->sym != PSA_ID)
      goto syntaxErr;
    newNode = TakeNode(items[0]);
    break;

  case 2:
    if (items[0]->sym != PSA_E ||
        (items[1]->sym != PSA_UMINUS && items[1]->sym != PSA_UPLUS))
      goto syntaxErr;
    // Action for: E -> u+ E / u- E
    if (!(newNode = NewNode(p, NODE_UNOP, items[0])))
      return false;
    newNode->unOp.operand = TakeNode(items[0]);
    newNode->unOp.op = (items[1]->sym == PSA_UMINUS) ? OP_UMINUS : OP_UPLUS;
    break;

  case 3:
    switch (items[2]->sym) {
    case PSA_LPAR:
      // E -> ( E )
      if (items[1]->sym != PSA_E || items[0]->sym != PSA_RPAR)
        goto syntaxErr;

      newNode = TakeNode(items[1]);
      break;

    case PSA_ID: {
      // E -> id ( )
      if (items[1]->sym != PSA_LPAR || items[0]->sym != PSA_RPAR ||
          items[2]->node->kind != NODE_ID)
        goto syntaxErr;
      // changes node ID to node FNCALL
      newNode = TakeNode(items[2]);
      char *name = newNode->name;
      newNode->kind = NODE_FNCALL;
      newNode->fncall.name = name;
      newNode->fncall.args = (ASTList_t){0};
      break;
    }

    case PSA_ARGS:
      // ARGS -> ARGS , E
      if (items[1]->sym != PSA_COMMA || items[0]->sym != PSA_E)
        goto syntaxErr;
      // takes helper node FNCALL, and appends E in the args list
      newNode = TakeNode(items[2]);
      if (!AppendNode(p, &newNode->fncall.args, items[0])) {
        ASTFree(newNode);
        return false;
      }
      newSym = PSA_ARGS;
      break;

    case PSA_E:
      switch (items[1]->sym) {
      case PSA_COMMA:
        // ARGS -> E , E
        if (items[0]->sym != PSA_E)
          goto syntaxErr;
        // Creates helper node FNCALL to then append E, E in the args list
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
        newSym = PSA_ARGS;
        break;

      case PSA_PLUS:
      case PSA_MINUS:
      case PSA_DIV:
      case PSA_MUL:
      case PSA_NEQ:
      case PSA_EQ:
      case PSA_GTE:
      case PSA_GT:
      case PSA_LTE:
      case PSA_LT:
        // E -> E op E
        // COND -> E op E ? i think we dont need it explicitely
        if (items[0]->sym != PSA_E)
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
    if (items[3]->sym != PSA_ID || items[2]->sym != PSA_LPAR ||
        items[0]->sym != PSA_RPAR || items[3]->node->kind != NODE_ID)
      goto syntaxErr;

    // E -> id ( E )
    if (items[1]->sym == PSA_E) {
      // changes node ID to node FNCALL and appends E in args list
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

    // E -> id ( ARGS )
    else if (items[1]->sym == PSA_ARGS) {
      // takes helper node FNCALL which already stores the arguments and
      // sets the FNCALL name to the name from node ID, then frees node ID
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

  // set the new stack top after the handle
  stack->top = current->next;
  // free node handle
  free(current);
  // free popped nodes from stack
  for (int i = 0; i < len; i++) {
    free(items[i]);
  }
  // push the new reduced item to stack
  if (!StackPush(stack, newSym, newNode)) {
    p->errorCode = ERROR_MEM;
    ASTFree(newNode);
    return false;
  }
  return true;

syntaxErr:
  p->errorCode = ERROR_SYNTAX;
  return false;
}

/* Parses nonterminal expression using precedence parsing.
 * returns AS tree of the expression if successful, else NULL. Error info in
 * p->errorCode
 * */
ASTNode_t *ParseExpression(Parser_t *p) {
  PStack_t stack;

  if (!StackInit(&stack)) {
    p->errorCode = ERROR_MEM;
    return NULL;
  }

  while (true) {
    if (p->current.type == TOKEN_ERROR) {
      if (p->errorCode == ERROR_NOERR)
        p->errorCode = p->current.error;

      goto stackFree;
    }

    PSASymbol_t topTerminal = StackTopTerminal(&stack)->sym;
    PSASymbol_t currentSymbol = SymFromToken(p->current.type);

    if (topTerminal == PSA_DOLLAR && currentSymbol == PSA_DOLLAR) {

      if (stack.top->sym != PSA_E || stack.top->next == NULL ||
          stack.top->next->sym != PSA_DOLLAR)
        goto failure;

      ASTNode_t *expr = TakeNode(stack.top);
      StackFree(&stack);
      return expr;
    }

    switch (PTable[topTerminal][currentSymbol]) {
    case '<':
      if (!StackInsertHandle(&stack)) {
        p->errorCode = ERROR_MEM;
        goto stackFree;
      }
    // fallthrough
    case '=': {
      ASTNode_t *node = NULL;
      if (currentSymbol == PSA_ID)
        if (!(node = CreateLeaf(p)))
          goto stackFree;

      if (!StackPush(&stack, currentSymbol, node)) {
        ASTFree(node);
        p->errorCode = ERROR_MEM;
        goto stackFree;
      }

      if (!Advance(p)) {
        goto stackFree;
      }
      break;
    }
      // ============================
    case '>':
      if (!Reduce(p, &stack))
        goto stackFree;
      break;

    default:
      goto failure;
    }
  }
failure:
  if (p->errorCode == ERROR_NOERR)
    p->errorCode = ERROR_SYNTAX;
stackFree:
  StackFree(&stack);
  return NULL;
}