/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 */
#ifndef IFJ26C_LOGGER_H
#define IFJ26C_LOGGER_H

#undef X
#define X_DLOG_LEVELS                                                          \
  X(TRACE)                                                                     \
  X(INFO)                                                                      \
  X(WARNING)                                                                   \
  X(ERROR)

#define X(x) DLOG_LVL_##x,
typedef enum { X_DLOG_LEVELS } DLogLevel_t;
#undef X

/* DebugLog()
 *   Logger enabled in DEBUG builds ('-DDEBUG').
 *
 *   'fmt' is equivalent to fprintf().
 *
 *   output: "<lvl>: <fmt>\n"
 */
void DebugLog(DLogLevel_t lvl, const char *fmt, ...);

/* Helpful macros: */
#ifdef DEBUG
#define DLOG_TRACE(...) DebugLog(DLOG_LVL_TRACE, __VA_ARGS__)
#define DLOG_INFO(...) DebugLog(DLOG_LVL_INFO, __VA_ARGS__)
#define DLOG_WARNING(...) DebugLog(DLOG_LVL_WARNING, __VA_ARGS__)
#define DLOG_ERROR(...) DebugLog(DLOG_LVL_ERROR, __VA_ARGS__)

#else /* ifdef DEBUG */
#define DLOG_NO_IMPL                                                           \
  do {                                                                         \
  } while (0)

#define DLOG_TRACE(...) DLOG_NO_IMPL
#define DLOG_INFO(...) DLOG_NO_IMPL
#define DLOG_WARNING(...) DLOG_NO_IMPL
#define DLOG_ERROR(...) DLOG_NO_IMPL

#endif /* ifdef DEBUG */

#endif
