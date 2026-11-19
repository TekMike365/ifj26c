/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 *
 * TODO: handle error tokens properly
 */
#include "parser_preprocessor.h"

#include "layout_filter.h"
#include "logger.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * Token Buffer
 */

#define TOKEN_BUFFER_SIZE 8 /* (ideally power of 2) > max lookahead */

#define DISTANCE(a, b) ((a) >= (b) ? (a) - (b) : (b) - (a))
#undef DISTANCE

typedef struct {
  size_t thisIdx;
  size_t nextIdx;
  Token_t tokens[TOKEN_BUFFER_SIZE];
} TokenBuffer_t;

static void TBInit(TokenBuffer_t *b) {
  assert(b);

  b->thisIdx = 0;
  b->nextIdx = 0;

  /* Clear all tokens. */
  for (size_t i = 0; i < TOKEN_BUFFER_SIZE; i++)
    b->tokens[i] = CreateToken();
}

static void TBTerminate(TokenBuffer_t *b) {
  if (!b)
    return;

  for (size_t i = 0; i < TOKEN_BUFFER_SIZE; i++)
    TerminateToken(&b->tokens[i]);
}

static size_t TBSize(TokenBuffer_t *b) {
  assert(b);

  /* 0 23   7 9 */
  /* ...n...t.. => 6 = 10 + 3 - 7 */
  /* ..t....n.. => 5 = 7 - 2 */
  /* ..nt...... => full: 9 = 10 + 3 - 2 */
  if (b->thisIdx > b->nextIdx)
    return TOKEN_BUFFER_SIZE + b->nextIdx - b->thisIdx;
  return b->nextIdx - b->thisIdx;
}

static bool TBIsFull(TokenBuffer_t *b) {
  assert(b);

  return TBSize(b) == TOKEN_BUFFER_SIZE - 1;
}

static Token_t TBGet(TokenBuffer_t *b, LayoutFilter_t *lf) {
  assert(b);

  if (TBSize(b) == 0) {
    TerminateToken(&b->tokens[b->nextIdx]);
    b->tokens[b->nextIdx] = GetNextToken(lf);
    b->nextIdx = (b->nextIdx + 1) % TOKEN_BUFFER_SIZE;
  }

  Token_t token = b->tokens[b->thisIdx];
  b->thisIdx = (b->thisIdx + 1) % TOKEN_BUFFER_SIZE;
  return token;
}

static void TBUnget(TokenBuffer_t *b) {
  assert(b);

  assert(!TBIsFull(b)); /* this shouldn't happen */

  b->thisIdx = (b->thisIdx + TOKEN_BUFFER_SIZE - 1) % TOKEN_BUFFER_SIZE;
}

/* TBReplace()
 *   Replaces current token with token `new'. The old token is not terminated.
 */
static void TBReplace(TokenBuffer_t *b, Token_t new) {
  assert(b);

  b->tokens[b->thisIdx] = new;
}

/* TBRemove()
 *   Removes the current token. The token gets terminated and `thisIdx' gets
 * pointed to the token after the removed one.
 */
static void TBRemove(TokenBuffer_t *b) {
  size_t i = b->thisIdx;
  TerminateToken(&b->tokens[i]); /* free the token */

  /* move all tokens back one */
  while (i != b->nextIdx) {
    size_t n = (i + 1) % TOKEN_BUFFER_SIZE;
    b->tokens[i] = b->tokens[n];
    i = n;
  }

  /* move the nextIdx back */
  b->nextIdx = (b->nextIdx + TOKEN_BUFFER_SIZE - 1) % TOKEN_BUFFER_SIZE;
}

/*
 * Preprocessor
 */

struct preprocessor {
  LayoutFilter_t *lf;
  TokenBuffer_t tb;

  struct {
    int blevel;
    int ilevel;
    enum {
      BFS_START = 0, /* start state */
      BFS_EINDENT,   /* expecting INDENT */
      BFS_INSIDE,    /* inside the indented block */
      BFS_COMMA,     /* supress comma if needed */
      BFS_EDEDENT,   /* expecting DEDENT */
    } state;
  } bf;
};

Preprocessor_t *PPCreate(void) {
  Preprocessor_t *p;
  p = malloc(sizeof *p);
  if (!p)
    return NULL;

  p->lf = LayoutFilterCreate(stdin);
  if (!p->lf) {
    free(p);
    return NULL;
  }

  TBInit(&p->tb);

  /* Bracket Filter Init */
  p->bf.blevel = 0;
  p->bf.ilevel = 0;
  p->bf.state = BFS_START;

  DLOG_INFO("<preprocessor:%p> initialized.", p);
  return p;
}

void PPDelete(Preprocessor_t *p) {
  if (!p)
    return;

  LayoutFilterTerminate(p->lf);
  TBTerminate(&p->tb);

  DLOG_INFO("<preprocessor:%p> terminated.", p);
  free(p);
}

/* this function works with the Token Buffer (you can get the return token by
 * calling `TBGet()'. */
static void FilterBrackets(Preprocessor_t *p) {
  assert(p);

  /* skip error tokens */
  Token_t t = TBGet(&p->tb, p->lf);
  TBUnget(&p->tb);
  if (t.type == TOKEN_ERROR)
    return;

  /* processing */
  switch (p->bf.state) {
  case BFS_START: {
    Token_t t = TBGet(&p->tb, p->lf);

    if (t.type == TOKEN_LPAR)
      p->bf.blevel++;
    else if (t.type == TOKEN_RPAR)
      p->bf.blevel--;

    if (p->bf.blevel > 0 && t.type == TOKEN_NEWLINE) {
      p->bf.state = BFS_EINDENT;
      break; /* eat the NEWLINE */
    }

    TBUnget(&p->tb);
  } break;
  case BFS_EINDENT: {
    Token_t t = TBGet(&p->tb, p->lf);

    if (t.type == TOKEN_INDENT) {
      p->bf.state = BFS_INSIDE;
      break; /* eat the INDENT */
    }

    /* missing indent */
    DLOG_ERROR("Error:%d: missing indent.", t.pos.line); /* TODO */
    p->bf.state = BFS_INSIDE;

    TBUnget(&p->tb);
  } break;
  case BFS_INSIDE: {
    Token_t t = TBGet(&p->tb, p->lf);

    if (t.type == TOKEN_LPAR)
      p->bf.blevel++;
    else if (t.type == TOKEN_RPAR)
      p->bf.blevel--;
    else if (t.type == TOKEN_INDENT) {
      p->bf.ilevel++;
      break; /* eat the INDENT */
    } else if (t.type == TOKEN_DEDENT) {
      p->bf.ilevel--;
      if (p->bf.ilevel < 0) {
        DLOG_ERROR("Error:%d: indent underflow (too many dedents)",
                   t.pos.line); /* TODO */
      }
      break; /* eat the DEDENT */
    }

    if (p->bf.blevel <= 0 && t.type == TOKEN_COMMA)
      p->bf.state = BFS_COMMA;
    else if (p->bf.blevel <= 0 && t.type == TOKEN_NEWLINE) {
      p->bf.state = BFS_EDEDENT;
      break; /* eat the NEWLINE */
    }

    TBUnget(&p->tb);
  } break;
  case BFS_COMMA: {
    TBGet(&p->tb, p->lf); /* pop COMMA token */
    Token_t t = TBGet(&p->tb, p->lf);

    if (p->bf.blevel <= 0 && t.type == TOKEN_NEWLINE) {
      p->bf.state = BFS_EDEDENT;
      break; /* eat the NEWLINE & the COMMA */
    }

    /* return both, the COMMA and the NEWLINE */
    TBUnget(&p->tb);
    TBUnget(&p->tb);
  } break;
  case BFS_EDEDENT: {
    Token_t t = TBGet(&p->tb, p->lf);

    if (p->bf.ilevel == 0 && t.type == TOKEN_DEDENT) {
      p->bf.state = BFS_START;
      break; /* eat the DEDENT */
    }

    /* Inside indent level errors */
    if (p->bf.ilevel > 0)
      DLOG_ERROR("Error:%d: indent overflow (missing dedents)",
                 t.pos.line); /* TODO */
    else if (p->bf.ilevel < 0)
      DLOG_ERROR("Error:%d: indent underflow (too many dedents)",
                 t.pos.line); /* TODO */
    p->bf.ilevel = 0;

    /* Skip DEDENT error */
    if (t.type == TOKEN_DEDENT) {
      p->bf.state = BFS_START;
      break;
    }

    /* missing dedent */
    p->bf.state = BFS_START;
    DLOG_ERROR("Error:%d: missing dedent.", t.pos.line); /* TODO */

    TBUnget(&p->tb);
  } break;
  }
}

/* this function works with the Token Buffer (you can get the return token by
 * calling `TBGet()'. */
static void ProcessUnaryOps(Preprocessor_t *p) {
  assert(p);

  Token_t t;

  t = TBGet(&p->tb, p->lf);
  TBUnget(&p->tb);
  if (t.type == TOKEN_ERROR)
    return;

  /* get prev token on the stack */
  TBUnget(&p->tb);
  t = TBGet(&p->tb, p->lf);

  /* replace if needed */
  switch (t.type) {
  case TOKEN_IDENTIFIER:
  case TOKEN_RPAR:
  case TOKEN_INT:
  case TOKEN_DOUBLE:
  case TOKEN_STRING:
  case TOKEN_UNIT:
    return; /* change nothing */
  default:
    /* get the token to be changed */
    t = TBGet(&p->tb, p->lf);

    if (t.type == TOKEN_PLUS)
      t.type = TOKEN_UNARY_PLUS;
    else if (t.type == TOKEN_MINUS)
      t.type = TOKEN_UNARY_MINUS;

    /* replace & return the token to be changed */
    TBUnget(&p->tb);
    TBReplace(&p->tb, t);
  }
}

Token_t PPNextToken(Preprocessor_t *p) {
  assert(p);

  FilterBrackets(p);
  ProcessUnaryOps(p);

  Token_t t = TBGet(&p->tb, p->lf);
  if (t.type == TOKEN_ERROR) {
    /* remove troublesome ERROR tokens */
    TBUnget(&p->tb);
    TBRemove(&p->tb);
  }

  return t;
}
