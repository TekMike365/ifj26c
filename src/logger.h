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
#define DLOG_TRACE(...)
#define DLOG_INFO(...)
#define DLOG_WARNING(...)
#define DLOG_ERROR(...)
#endif /* ifdef DEBUG */

#endif
