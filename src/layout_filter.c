/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "layout_filter.h"
#include "lexer.h"
#include "token.h"
#include <assert.h>

// ##############################################################
//                LAYOUT_FILTER.C INTERNAL STUFF
// ##############################################################

/* Function tries to put 'indent' on top of the indent stack
 * * indent - incoming indent to validate
 * * indentStack - stack of active indents
 * * levelsChanged - out parameter, count of in/dedent
 * * returns 1 on sucess, else 0
 * if 'indent' is greater than top, indent increased
 * if 'indent' is smaller, it is a dedent -> pop until matching indent is found
 * if not found, there was no matching indent -> error
 * */
static ErrorCode_t UpdateIndentStack(int indent, IntStack_t *indents,
                                     int *levelsChanged) {
  assert(indents != NULL);

  if (IntStackIsEmpty(indents)) {
    return ERROR_INTERNAL;
  }

  int change = 0; // level change, zero initially

  // top is smaller -> indent, push
  if (IntStackTop(indents) < indent) {
    if (!IntStackPush(indents, indent)) {
      return ERROR_INTERNAL;
    };
    change++; // indent increased
  }
  // else dedent -> try to put it on top of the stack
  // pop until indent is smaller than top and stack is not empty
  else {
    if (!IntStackContains(indents, indent)) {
      return ERROR_LEX;
    }
    int top;
    while ((top = IntStackTop(indents)) != 0 && top > indent) {
      IntStackPop(indents);
      change--; // indent decreased
    }
  }

  if (levelsChanged) {
    *levelsChanged = change;
  }
  // success
  return ERROR_NOERR;
}

// store a token inside LayoutFilter`s stash
static inline void LayoutFilterStashToken(LayoutFilter_t *filter,
                                          Token_t token) {
  assert(filter != NULL);

  filter->hasStash = 1;
  filter->stash = token;
}

// pop a stashed token
static inline Token_t LayoutFilterUnstashToken(LayoutFilter_t *filter) {
  assert(filter != NULL);

  filter->hasStash = 0;
  return filter->stash;
}

// ##############################################################
//                LAYOUT_FILTER.H IMPLEMENTATION
// ##############################################################

/* Initialize LayoutFilter
 * * filter - LayoutFilter to be initialized
 * * src - source file for Lexer_t
 * Pushes 0 on indentation stack and may thus fail on memory
 * returns 0 on mem failure or NULL args, else 1
 */
_Bool LayoutFilterInit(LayoutFilter_t *filter, FILE *src) {
  if (!filter || !src) {
    return 0;
  }

  filter->lexer = LexerCreate(src);

  if (!filter->lexer) {
    return 0;
  }

  filter->indents = IntStackCreate();
  if (!filter->indents) {
    return 0;
  }
  // push 0 on the stack
  // acts as a reference value that is never popped
  // until terminated
  if (!IntStackPush(filter->indents, 0)) {
    return 0;
  }
  filter->stash = (Token_t){0};
  filter->hasStash = 0;

  filter->atLineStart = 1;
  filter->dedentQueue = 0;
  filter->expectEOL = 0;

  filter->nextIndent = (IndentInfo_t){0};

  filter->bracketDepth = 0;
  return 1;
}

/* terminate */
void LayoutFilterTerminate(LayoutFilter_t *filter) {
  if (!filter) {
    return;
  }
  LexerTerminate(filter->lexer);
  IntStackTerminate(filter->indents);
  filter->stash = (Token_t){0};
  filter->hasStash = 0;

  filter->atLineStart = 1;
  filter->dedentQueue = 0;

  filter->expectEOL = 0;
  filter->nextIndent = (IndentInfo_t){0};

  filter->bracketDepth = 0;
}

/* Wrapper function that encapsulates Lexer, Indent stack and flags
 * Acts as a token interceptor/filter layer above lexer, but below parser
 * LEXER -> INTERCEPTOR -> PARSER
 * asks for tokens in a loop until it can emit a meaningful token
 * to decide, it uses flags and helper structures
 * handles correct TOKEN_NEWLINE emission, indentation and EOF
 * emits synthetic tokens as well (i.e. TOKEN_NEWLINE before TOKEN_EOF)
 */
Token_t GetNextToken(LayoutFilter_t *filter) {
  // return TOKEN_NONE
  if (!filter) {
    return CreateToken();
  }

  if (filter->dedentQueue > 0) {
    filter->dedentQueue--;
    return MakeToken(TOKEN_DEDENT);
  }

  if (filter->hasStash) {
    return LayoutFilterUnstashToken(filter);
  }

  Token_t new;

  // get new tokens, return only relevant ones
  for (;;) {
    new = GetRawToken(filter->lexer);

    // if TOKEN_ERROR
    // already reported by lexer
    if (new.type == TOKEN_ERROR) {
      return new;
    }

    // if TOKEN_EXPECT_EOL
    // set flag and discard it
    if (new.type == TOKEN_EXPECT_EOL) {
      filter->expectEOL = 1;
      continue;
    }

    // if TOKEN_NEWLINE
    if (new.type == TOKEN_NEWLINE) {

      // last multiline comment line ended
      filter->expectEOL = 0;
      filter->nextIndent = new.indent;
      // if it doesnt come from a logical line
      // skip
      if (filter->atLineStart) {
        continue;
      }

      filter->atLineStart = 1;

      return new;
    }

    // if TOKEN_EOF
    if (new.type == TOKEN_EOF) {
      // emit NEWLINE on logical line before EOF
      // to end command
      if (!filter->atLineStart) {
        filter->atLineStart = 1;
        return MakeToken(TOKEN_NEWLINE);
      }
      // set valid indent to dedent all blocks
      filter->nextIndent = (IndentInfo_t){0};
    }

    if (new.type == TOKEN_LPAR) {
      filter->bracketDepth++;
    }

    if (new.type == TOKEN_RPAR) {
      filter->bracketDepth--;
    }

    // expected EOL after multiline block comment, but other token came
    // error
    if (filter->expectEOL) {
      // set it here to avoid repeated error for every token on that line
      filter->expectEOL = 0;

      ReportLexicalError(
          MSG_CONTEXT_T(.lineNum = new.pos.line, .hlStartCol = new.pos.start,
                        .hlEndCol = new.pos.end, .poiCol = 0),
          LexerGetBuf(filter->lexer),
          "No token allowed on the last line of a block comment.", 1);

      LayoutFilterStashToken(filter, new);
      Token_t err = CreateToken();
      TokenSetError(ERROR_LEX, &err);
      return err;
    }

    // if token after empty space
    // if it got here, it has to be logical token
    if (filter->atLineStart) {

      // no longer at start
      filter->atLineStart = 0;
      // if indent had tab, error
      if (filter->nextIndent.hasTab) {

        // newline has different, but
        // since it carries indent, which
        // always starts at 1, let me do
        // it this way
        ReportLexicalError(
            MSG_CONTEXT_T(.lineNum = new.pos.line, .hlStartCol = 1,
                          .hlEndCol = new.pos.end, .poiCol = 0),
            LexerGetBuf(filter->lexer),
            "Tabulator can`t be used to indent a logical line.", 1);

        LayoutFilterStashToken(filter, new);

        Token_t err = CreateToken();
        TokenSetError(ERROR_LEX, &err);
        return err;
      }

      // normal indent processing
      int change;

      ErrorCode_t error =
          UpdateIndentStack(filter->nextIndent.width, filter->indents, &change);
      if (error != ERROR_NOERR) {

        if (error == ERROR_INTERNAL) {
          ReportInternalError("Indent stack push failure: Out of memory.");
        }

        ReportLexicalError(MSG_CONTEXT_T(.lineNum = new.pos.line,
                                         .hlStartCol = new.pos.start,
                                         .hlEndCol = new.pos.end, .poiCol = 0),
                           LexerGetBuf(filter->lexer),
                           "No matching indent for dedent found.", 1);

        LayoutFilterStashToken(filter, new);

        Token_t err = CreateToken();
        TokenSetError(error, &err);
        return err;
      }

      if (change != 0) {

        LayoutFilterStashToken(filter, new);

        if (change > 0) {
          return MakeToken(TOKEN_INDENT);
        }
        // change is negative on dedent, so -change
        // -1 because one TOKEN_DEDENT is emitted right now
        filter->dedentQueue = -change - 1;
        return MakeToken(TOKEN_DEDENT);
      }
    }
    // logical token after boundary between line start
    // just return it
    return new;
  }
}
