/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#include "logger.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG

#define X(x) #x,
static const char *const m_levelStrs[] = {X_DLOG_LEVELS};
#undef X

void DebugLog(DLogLevel_t lvl, const char *fmt, ...) {
  assert(fmt);

  /* build log string: "<lvl>: <fmt>\n\0"*/
  const size_t logLen =
      strlen(fmt) + strlen(m_levelStrs[lvl]) + 4 /* ": " + "\n\0" */;
  char log[logLen];
  log[0] = 0;
  strcat(log, m_levelStrs[lvl]);
  strcat(log, ": ");
  strcat(log, fmt);
  strcat(log, "\n");

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, log, ap);
  va_end(ap);
}

#else /* ifdef DEBUG */

void DebugLog(DLogLevel_t lvl, const char *fmt, ...) { (void)lvl, (void)fmt; }

#endif /* ifdef DEBUG */
