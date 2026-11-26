/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 *            Ondrej Dedič  <xdedico00>
 * */

// ###################################################
//                     DESCRIPTION
// ###################################################
/* Module defines error codes as per spec and provides
 * functions for reporting compiler errors via
 * messenger.h
 */
// ###################################################

#ifndef IFJ26_ERROR_H
#define IFJ26_ERROR_H

#include "messenger.h"
#include <stddef.h>

// error codes for compiler
typedef enum {
  ERROR_NOERR = 0,         // no error
  ERROR_LEX = 1,           // lexical error
  ERROR_SYNTAX = 2,        // syntax error
  ERROR_SEM_UNDEF = 3,     // undefined function, variable, @main
  ERROR_SEM_REDEF = 4,     // function, variable or type redefinition
  ERROR_SEM_ARGS = 5,      // invalid argument number/type
  ERROR_SEM_TYPE = 6,      // type compatibility error
  ERROR_SEM_IMMUTABLE = 7, // prohibited write (immutable)
  ERROR_SEM_OTHER = 10,    // other semantic errors
  ERROR_INTERNAL = 99,     // internal error (mem and such)
} ErrorCode_t;

void ReportLexicalError(MsgContext_t ctx, const char *code, const char *msg,
                        _Bool addContext);

void ReportInternalError(const char *msg);

void ReportSyntaxError(MsgContext_t ctx, const char *msg, const char *detail,
                       _Bool addContext);

#endif // IFJ26_ERROR_H
