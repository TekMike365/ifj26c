/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "messenger.h"

#include "logger.h"
#include <assert.h>
#include <errno.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ctxNode CtxNode_t;
struct ctxNode {
  /* linked list stuff */
  CtxNode_t *next;

  /* Data */
  MsgContext_t ctx; /* public Interface */
  char msg[MAX_MSG_LENGTH + 1];
};

struct message {
  MsgType_t type;
  int errCode;
  CtxNode_t *ctxFirst;
  CtxNode_t *ctxLast;
  char msg[MAX_MSG_LENGTH + 1];
};

#define TO_STR(x) #x,
static const char *const m_msgTypeStrs[] = {FOR_MESSAGE_TYPE(TO_STR)};
#undef TO_STR

static struct {
  size_t size;
  size_t cap;
  long *data;
} m_lineOffsetVec;

void MessengerInit(void) {
  DLOG_INFO("<messenger> Initialising.");
  m_lineOffsetVec.size = 0;
  m_lineOffsetVec.cap = 0;
  m_lineOffsetVec.data = NULL;

  errno = 0;
  if (ftell(stdin) < 0 && errno == ESPIPE) {
    DLOG_INFO("<messenger> Input file isn't seekable.");
    return; /* file isn't seekable (don't bother to init m_lineOffsetVec) */
  }

  /* init line offsets */
  m_lineOffsetVec.cap = 10;
  m_lineOffsetVec.data =
      malloc(sizeof *m_lineOffsetVec.data * m_lineOffsetVec.cap);
  if (!m_lineOffsetVec.data)
    DLOG_WARNING("<messenger> Failed to allocate line offsets array.");
}

void MessengerTerminate(void) {
  DLOG_INFO("<messenger> Terminating.");
  free(m_lineOffsetVec.data);
}

long MessengerStoreNewLine(long offset) {
  if (!m_lineOffsetVec.data)
    return -1;

  /* resize if needed */
  if (m_lineOffsetVec.size == m_lineOffsetVec.cap) {
    DLOG_TRACE("<messenger> Reallocating line offset buffer.");
    m_lineOffsetVec.cap = m_lineOffsetVec.cap * 3 / 2;
    m_lineOffsetVec.data = realloc(m_lineOffsetVec.data, m_lineOffsetVec.cap);

    if (!m_lineOffsetVec.data) {
      DLOG_WARNING("<messenger> Failed to reallocate line offset buffer.");
      return -1;
    }
  }

  /* append the offset */
  m_lineOffsetVec.data[m_lineOffsetVec.size++] = offset;
  return m_lineOffsetVec.size;
}

Message_t *MsgStart(MsgType_t type, int errCode, const char *fmt, ...) {
  assert(fmt);

  /* allocate message */
  Message_t *msg;
  msg = malloc(sizeof *msg);
  if (!msg)
    return NULL;

  /* init message */
  va_list ap;
  va_start(ap, fmt);

  msg->type = type;
  msg->errCode = errCode;
  msg->ctxFirst = NULL;
  msg->ctxLast = NULL;
  vsnprintf(msg->msg, MAX_MSG_LENGTH + 1, fmt, ap);

  va_end(ap);

  DLOG_TRACE("<msg:%p> Message created.", msg);
  return msg;
}

void MsgDrop(Message_t *msg) {
  if (!msg)
    return;

  DLOG_TRACE("<msg:%p> Dropping message.", msg);

  /* free list */
  CtxNode_t *e = msg->ctxFirst;
  while (e) {
    DLOG_TRACE("<msg:%p-ctx:%p> Deleting context.", msg, e);
    CtxNode_t *next = e->next;
    free(e);
    e = next;
  }

  free(msg);
}

MsgErr_t TryPrintCtxLine(FILE *fp, MsgContext_t *ctx) {
  assert(fp && ctx && ctx->lineNum > 0);

  if (!m_lineOffsetVec.data)
    return MSG_ERR_OK; /* don't have offset info */

  errno = 0;
  long oldPos = ftell(fp);
  if (oldPos < 0) {
    if (errno = ESPIPE)
      DLOG_INFO("<msg-ctx> Failed to get current file position, file isn't "
                "seekable.");
    else
      DLOG_INFO("<msg-ctx> Failed to get current file position.");
    return MSG_ERR_OK;
  }

  if (m_lineOffsetVec.size <= ctx->lineNum - 1) {
    DLOG_WARNING(
        "<msg-ctx> Line offset vector doesn't contain the line number `%d'.",
        ctx->lineNum);
    return MSG_ERR_OK;
  }

  long begin = m_lineOffsetVec.data[ctx->lineNum - 1];

  errno = 0;
  if (fseek(fp, begin, SEEK_SET) < 0) {
    DLOG_WARNING(
        "<msg-ctx> Failed to do an `fseek' to `%d' (old: `%d') with errno: %s",
        begin, oldPos, strerror(errno));
    return MSG_ERR_OK;
  }

  /* print whole line */
  fputs("  |", stderr);
  for (int c; (c = fgetc(fp)) != '\n' && c != EOF;)
    fputc(c, stderr);
  fputc('\n', stderr);

  if (fseek(fp, oldPos, SEEK_SET) < 0) {
    DLOG_ERROR("<msg-ctx> Failed to return to file pos `%d' with errno: %s",
               oldPos, strerror(errno));
    return MSG_ERR_NORETURN;
  }

  /* print decoration */
  fputs("  |", stderr);
  for (size_t i = 1; i <= ctx->hlEndCol || i <= ctx->poiCol; i++) {
    if (ctx->poiCol > 0 && i == ctx->poiCol)
      fputc('^', stderr);
    else if (ctx->hlStartCol > 0 && i > ctx->hlStartCol && i <= ctx->hlEndCol)
      fputc('~', stderr);
    else
      fputc(' ', stderr);
  }
  fputc('\n', stderr);

  return MSG_ERR_OK;
}

MsgErr_t MsgSend(Message_t *msg) {
  if (!msg)
    return MSG_ERR_FAILED;

  DLOG_TRACE("<msg:%p> Sending message.", msg);
  fprintf(stderr, "[%s] (E%d): %s\n", m_msgTypeStrs[msg->type], msg->errCode,
          msg->msg);

  MsgErr_t error = MSG_ERR_OK;

  /* print full context */
  for (CtxNode_t *e = msg->ctxFirst; e; e = e->next) {
    /* print ctx short */
    if (e->ctx.lineNum == 0) {
      fprintf(stderr, "  %s\n", e->msg);
      continue;
    }

    /* print ctx long */
    size_t col = e->ctx.poiCol > 0 ? e->ctx.poiCol : e->ctx.hlStartCol;
    fprintf(stderr, "  %zu:", e->ctx.lineNum);
    if (col > 0)
      fprintf(stderr, "%zu:", col);
    fprintf(stderr, " %s\n", e->msg);

    error = TryPrintCtxLine(stdin, &e->ctx);
  }

  MsgDrop(msg);
  return error;
}

void MsgAddContext(Message_t *msg, MsgContext_t ctx, const char *fmt, ...) {
  assert(fmt);

  if (!msg)
    return;

  CtxNode_t *new;
  new = malloc(sizeof *new);
  if (!new)
    return;

  /* init */
  va_list ap;
  va_start(ap, fmt);

  new->next = NULL;
  vsnprintf(new->msg, MAX_MSG_LENGTH + 1, fmt, ap);
  new->ctx = ctx;

  va_end(ap);

  /* add to list */
  if (msg->ctxFirst == NULL) {
    msg->ctxFirst = new;
    msg->ctxLast = new;
  } else {
    msg->ctxLast->next = new;
    msg->ctxLast = new;
  }

  DLOG_TRACE("<msg:%p-ctx:%p> Context added.", msg, new);
}
