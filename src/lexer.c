/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "char_buf.h"
#include "error.h"
#include "lexer_fsm.h"
#include "messenger.h"
#include "token.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

struct Lexer {
  FILE *src;      // source file
  CharBuf_t *buf; // buffer for read characters
  size_t offset;  // file offset
  size_t line;    // counts newlines
  size_t col;     // line offset

  // stores the previous character
  // used to replace ungetc
  // next invalid char is delimiter to token
  int prevChar;
};

/**TODO:
 * string pool
 * make TRANSITION TABLE 1D
 */

// ##############################################################
//                    LEXER.C INTERNAL STUFF
// ##############################################################

/* Compares suffix with reference suffix (keyword suffix) and returns user
 * passed type on match, else defaults to TOKEN_IDENTIFIER
 * * str - source lexeme suspicious of being a keyword
 * * ref - suffix of the keyword
 * * len - length of the suffix
 * * matched - token type returned on match
 */
static inline TokenType_t MatchKeywordSuffix(const char *str, const char *ref,
                                             size_t len, TokenType_t matched) {
  assert(str != NULL);
  assert(ref != NULL);
  if (memcmp(str, ref, len) == 0) {
    return matched;
  }

  return TOKEN_IDENTIFIER;
}

/* decides whether str is identifier or keyword
 * implemented using length-partition followed by trie matching
 * * str - pointer to a char array
 * return - TOKEN_NONE on str == NULL, TOKEN_KW_X on keyword X, else
 * TOKEN_IDENTIFIER
 */
static TokenType_t ParseIdentifier(const char *str) {
  if (!str)
    return TOKEN_NONE;

  size_t len = strlen(str);

  // shortest keyword (do/if) has len 2
  // longest (extension) has 9
  if (len < 2 || len > 9) {
    return TOKEN_IDENTIFIER;
  }
  // partition keyword set by length
  switch (len) {
  case 2:
    // trie search
    switch (str[0]) {
    case 'b':
      // match the rest with "y" as in "by"
      //                    rest has len 1 ---+
      //                    		      |
      //                                      v
      return MatchKeywordSuffix(str + 1, "y", 1, TOKEN_KW_BY); // 'by'
    case 'd':
      return MatchKeywordSuffix(str + 1, "o", 1, TOKEN_KW_DO); // 'do'
    case 'i':
      return MatchKeywordSuffix(str + 1, "f", 1, TOKEN_KW_IF); // 'if'
    case 't':
      return MatchKeywordSuffix(str + 1, "o", 1, TOKEN_KW_TO); // 'to'
    }
    break;
  case 3:
    switch (str[0]) {
    case 'd':
      return MatchKeywordSuffix(str + 1, "ef", 2, TOKEN_KW_DEF); // 'def'
    case 'f':
      return MatchKeywordSuffix(str + 1, "or", 2, TOKEN_KW_FOR); // 'for'
    case 'I':
      return MatchKeywordSuffix(str + 1, "nt", 2, TOKEN_KW_INT); // 'Int'
    case 'v':
      switch (str[1]) {
        // annoying, both ('var'/'val') start as 'va' and have length 3 :(
        // kept switch for consistency
      case 'a':
        switch (str[2]) {
        case 'l':
          return TOKEN_KW_VAL; // 'val'

        case 'r':
          return TOKEN_KW_VAR; // 'var'
        }
      }
    }
    break;

  case 4:
    switch (str[0]) {
    case 'c':
      return MatchKeywordSuffix(str + 1, "ase", 3,
                                TOKEN_KW_CASE); // 'case'
    case 'e':
      return MatchKeywordSuffix(str + 1, "lse", 3,
                                TOKEN_KW_ELSE); // 'else'
    case 'L':
      return MatchKeywordSuffix(str + 1, "ist", 3,
                                TOKEN_KW_LIST); // 'List'
    case 't':
      switch (str[1]) {
      case 'h':
        return MatchKeywordSuffix(str + 2, "en", 2,
                                  TOKEN_KW_THEN); // 'then'
      case 'y':
        return MatchKeywordSuffix(str + 2, "pe", 2,
                                  TOKEN_KW_TYPE); // 'type'
      case 'r':
        return MatchKeywordSuffix(str + 2, "ue", 2,
                                  TOKEN_KW_BOOL_TRUE); // 'true'

      default:
        return TOKEN_IDENTIFIER;
      }
    case 'U':
      return MatchKeywordSuffix(str + 1, "nit", 3,
                                TOKEN_KW_UNIT); // 'Unit'
    }
    break;

  case 5:
    switch (str[0]) {
    case 'c':
      return MatchKeywordSuffix(str + 1, "lass", 4,
                                TOKEN_KW_CLASS); // 'class'
    case 'm':
      return MatchKeywordSuffix(str + 1, "atch", 4,
                                TOKEN_KW_MATCH); // 'match'
    case 'u':
      return MatchKeywordSuffix(str + 1, "ntil", 4,
                                TOKEN_KW_UNTIL); // 'until'
    case 'w':
      return MatchKeywordSuffix(str + 1, "hile", 4,
                                TOKEN_KW_WHILE); // 'while'
    case 'f':
      return MatchKeywordSuffix(str + 1, "alse", 4,
                                TOKEN_KW_BOOL_FALSE); // 'false'
    }
    break;
  case 6:
    switch (str[0]) {
    case 'D':
      return MatchKeywordSuffix(str + 1, "ouble", 5,
                                TOKEN_KW_DOUBLE); // 'Double'
    case 'i':
      return MatchKeywordSuffix(str + 1, "mport", 5,
                                TOKEN_KW_IMPORT); // 'import'
    case 'r':
      return MatchKeywordSuffix(str + 1, "eturn", 5,
                                TOKEN_KW_RETURN); // 'return'
    case 'S':
      return MatchKeywordSuffix(str + 1, "tring", 5,
                                TOKEN_KW_STRING); // 'String'
    }
    break;
  case 7:
    return MatchKeywordSuffix(str + 0, "Boolean", 7,
                              TOKEN_KW_BOOLEAN); // 'Boolean'
    // no keyword with length 8
  case 8:
    return TOKEN_IDENTIFIER;
  case 9:
    return MatchKeywordSuffix(str + 0, "extension", 9,
                              TOKEN_KW_EXTENSION); // 'extension'

  default:
    return TOKEN_IDENTIFIER;
  }
  return TOKEN_IDENTIFIER;
}

static inline int GetNextChar(Lexer_t *lexer) {
  assert(lexer != NULL);
  // if prevChar is nonzero, reset and return it
  if (lexer->prevChar != 0) {
    int c = lexer->prevChar;
    lexer->prevChar = 0;
    return c;
  }
  // else read new char from stream
  int c = fgetc(lexer->src);
  // if new line, increment line count
  if (c == '\n') {
    lexer->line++;
    lexer->col = 1;
    MessengerStoreNewLine(ftell(lexer->src));
  }
  // if not EOF, increment offset
  if (c != EOF) {
    lexer->offset++;
    lexer->col++;
  }
  // return
  return c;
}

/* Converts a string [+- ][0-9_]* into a decimal int
 * !!! DESTRUCTIVE -- overwrites '_' (i.e. 1_2_34->1234)
 * nptr - pointer to the start of the number string
 * end - (may be NULL) pointer to the last read char
 * return - converted int
 */
static int StrToInt(char *restrict nptr, char **restrict end) {
  assert(nptr != NULL);

  size_t len = strlen(nptr);
  char *c = nptr;
  for (size_t idx = 0; idx < len; idx++) {
    if (nptr[idx] != '_') {
      *(c++) = nptr[idx];
    }
  }
  *c = '\0';

  return strtol(nptr, end, 10);
}

/* Converts a string of double into a decimal int
 * !!! DESTRUCTIVE -- overwrites '_' (i.e. 1.2_34e56->1.234e56)
 * nptr - pointer to the start of the number string
 * end - (may be NULL) pointer to the last read char
 * return - converted int
 */
static double StrToDouble(char *restrict nptr, char **restrict end) {
  assert(nptr != NULL);

  size_t len = strlen(nptr);
  char *c = nptr;
  for (size_t idx = 0; idx < len; idx++) {
    if (nptr[idx] != '_') {
      *(c++) = nptr[idx];
    }
  }
  *c = '\0';

  return strtod(nptr, end);
}

/* Function skips block comment
 * If comment spans one line, it is replaced by ' '
 * else with newline
 * if last line continues with non-whitespace, error
 * * isMultiline - optional argument
 * */
static _Bool SkipBlockComment(Lexer_t *lexer, _Bool *isMultiline) {
  assert(lexer != NULL);

  // previous char, avoids ungetc()
  int prev = 0;
  int c;

  // does it span multiple lines?
  _Bool hasNewline = 0;

  // called after consuming "/*"
  // hence depth is implicitly 1
  unsigned depth = 1;

  // while in block comment and not at the end
  while (depth > 0 && (c = GetNextChar(lexer)) != EOF) {

    if (c == '\n') {
      hasNewline = 1;
    }
    // "/*"
    if (prev == '/' && c == '*') {
      // reset c, otherwise "/*/" will trigger depth++ for "/*" and then
      // -- for "*/", even though "*/" is not an end of nesting level
      c = 0;
      depth++;
    }
    // "*/"
    else if (prev == '*' && c == '/') {
      // same here for "*/*"
      c = 0;
      depth--;
    }
    prev = c;
  }

  // if contained newline, it is multiline
  if (isMultiline != NULL) {
    *isMultiline = hasNewline;
  }

  // if multi line block comment, return c
  // else replace the comment by space
  lexer->prevChar = hasNewline ? '\n' : ' ';

  // return: 1 => skipped without problem; 0 => EOF before comment end
  return depth == 0;
}

// skips line comment, like this one
static inline void SkipLineComment(Lexer_t *lexer) {
  assert(lexer != NULL);

  int c;
  // read until EOF or line feed
  while ((c = GetNextChar(lexer)) != EOF && c != '\n')
    ;

  // return c to "stream"
  lexer->prevChar = c;
}

// skips whitespace excluding '\n'
static inline void SkipWhitespace(Lexer_t *lexer) {
  assert(lexer != NULL);

  int c;

  // whitespace as allowed per spec
  while ((c = GetNextChar(lexer)) == ' ' || c == '\t' || c == '\r')
    ;

  lexer->prevChar = c;
}

// returns 0 if indent contains \t, else 1
// function counts spaces even after seeing tab, because:
// if on empty line, it will be discarded
// if on logical line, it is a lexical error anyway ->
// indent won`t be used
static inline IndentInfo_t MeasureIndent(Lexer_t *lexer) {
  assert(lexer != NULL);

  int width = 0;
  _Bool hasTab = 0;
  int c;

  while ((c = GetNextChar(lexer)) == ' ' || c == '\t') {
    if (c == '\t') {
      hasTab = 1;
    }

    width++;
  }

  lexer->prevChar = c;
  return (IndentInfo_t){.width = width, .hasTab = hasTab};
}

/* Function is the direct implementation of the lexer FSM
 * It handles:
 * mapping ASCII char -> CHAR_TYPE
 * mapping the tuple (CHAR_TYPE, CURRENT STATE) ->  NEXT STATE
 * maps END STATE -> TOKEN_TYPE
 * looks up whether identifier is keyword
 * the token is "raw" in the sense that it outputs a token
 * irrespective of context (that is every NL, every indent, ...)
 * emits a special token TOKEN_EXPECT_EOL, used to validate whether
 * the last line of a block comment contains no token (except for newline)
 * note: the validation is not done here
 * converts int and double string literal to numerical value
 * saves identifiers and string literals
 * all other tokens are recognizable by their TokenType
 * */
Token_t GetRawToken(Lexer_t *lexer) {
  if (!lexer) {
    return MakeToken(TOKEN_NONE);
  }
  // used for multiline comments to emit TOKEN_EXPECT_EOL
  _Bool isMultiline;
  // create new token
  Token_t new = CreateToken();

  State_t current, next;

RESET:
  // on EOF, return TOKEN_EOF
  if (lexer->prevChar == EOF) {
    new.type = TOKEN_EOF;
    return new;
  }

  new.pos.line = lexer->line; // set line
  new.pos.start = lexer->col; // set start

  // reset buffer
  CharBufClear(lexer->buf);

  current = STATE_START;

  int c;
  while ((c = GetNextChar(lexer)) != EOF) {

    CharType_t type = MapChar(c);

    next = NextState(current, type);
    if (next == STATE_END) {
      break;
    }

    if (!CharBufInsert(lexer->buf, c)) {
      TokenSetError(ERROR_INTERNAL, &new);
      ReportInternalError("Out of memory.");
      return new;
    }

    current = next;
  }

  // next char was consumed to determine end
  // i.e. '+' in "pos+1" is a delimiter for "pos"
  // must push it back into "stream"
  // prevChar replaces ungetc
  lexer->prevChar = c;

  // can be optimized by STATE->TOKEN_TYPE map
  switch (current) {

    // transition table is initalized so that on unknow symbol,
    // STATE_START goes to STATE_ERROR
    // this case shouldn`t occurr
  case STATE_START:
    new.type = TOKEN_ERROR;
    ReportInternalError("Lexer terminated without reading characters.");
    break;

  case STATE_IDENTIFIER:
    // IF STRING POOL CONTAINED STRING+METADATA i.e. TOKEN POOL
    // Token type could be a part of it?
    // ParseIdentifier not needed?
    new.type = ParseIdentifier(CharBufGetBuf(lexer->buf));
    if (new.type == TOKEN_IDENTIFIER) {
      if (!TokenSetLexemeS(&new, CharBufGetBuf(lexer->buf),
                           CharBufGetSize(lexer->buf))) {
        TokenSetError(ERROR_INTERNAL, &new);
        ReportInternalError("Out of memory.");
        return new;
      }
    }
    break;

  case STATE_EQ:
    new.type = TOKEN_EQ;
    break;
  case STATE_NOT_EQ:
    new.type = TOKEN_NOT_EQ;
    break;
  case STATE_DOUBLE_EQ:
    new.type = TOKEN_DOUBLE_EQ;
    break;
  case STATE_GREATER:
    new.type = TOKEN_GREATER;
    break;
  case STATE_GREATER_EQ:
    new.type = TOKEN_GREATER_EQ;
    break;
  case STATE_LESS:
    new.type = TOKEN_LESS;
    break;
  case STATE_LESS_EQ:
    new.type = TOKEN_LESS_EQ;
    break;
  case STATE_LEFT_ARROW:
    new.type = TOKEN_LEFT_ARROW;
    break;
  case STATE_STAR:
    new.type = TOKEN_STAR;
    break;
  case STATE_PLUS:
    new.type = TOKEN_PLUS;
    break;
  case STATE_MINUS:
    new.type = TOKEN_MINUS;
    break;
  case STATE_SLASH:
    new.type = TOKEN_SLASH;
    break;

  case STATE_WHITESPACE:
    SkipWhitespace(lexer);
    goto RESET;

  case STATE_COMMENT:
    SkipLineComment(lexer);
    // comment not a token, start anew
    goto RESET;

  case STATE_BLOCK_COMMENT:
    // if unclosed
    if (!SkipBlockComment(lexer, &isMultiline)) {
      ReportLexicalError(
          MSG_CONTEXT_T(.lineNum = lexer->line, .hlStartCol = lexer->col,
                        .hlEndCol = lexer->col, .poiCol = 0),
          CharBufGetBuf(lexer->buf), "Unclosed block comment.", 1);
      TokenSetError(ERROR_LEX, &new);
      break;
    }
    // if correct but multiline, a special token is emitted
    // layout validates whether no token is on the comment`s last line
    if (isMultiline) {
      new.type = TOKEN_EXPECT_EOL;
      break;
    }
    // else, discard and start anew
    goto RESET;

  case STATE_LPAR:
    new.type = TOKEN_LPAR;
    break;

  case STATE_RPAR:
    new.type = TOKEN_RPAR;
    break;

  case STATE_AT:
    new.type = TOKEN_AT;
    break;

  case STATE_AND:
    new.type = TOKEN_AND;
    break;

  case STATE_OR:
    new.type = TOKEN_OR;
    break;

  case STATE_EXCL_MARK:
    new.type = TOKEN_EXCL_MARK;
    break;

  case STATE_ZERO:
    new.type = TOKEN_INT;
    new.lexemeI = 0;
    break;

  case STATE_INT:
    new.type = TOKEN_INT;
    new.lexemeI = StrToInt(CharBufGetBuf(lexer->buf), NULL);
    break;

  case STATE_DOUBLE:
  case STATE_EXPONENT:
  case STATE_END_DOUBLE:
    new.type = TOKEN_DOUBLE;
    new.lexemeD = StrToDouble(CharBufGetBuf(lexer->buf), NULL);
    break;

  case STATE_DOT:
    new.type = TOKEN_DOT;
    break;

  case STATE_SEMICOLON:
    new.type = TOKEN_SEMICOLON;
    break;

  case STATE_COMMA:
    new.type = TOKEN_COMMA;
    break;

  case STATE_COLON:
    new.type = TOKEN_COLON;
    break;

  case STATE_DQUOTE:
  case STATE_END_STRING:
  case STATE_END_MULTILINE_STR:
    new.type = TOKEN_STRING;

    // set lexemeS
    if (!TokenSetLexemeS(&new, CharBufGetBuf(lexer->buf),
                         CharBufGetSize(lexer->buf))) {
      TokenSetError(ERROR_INTERNAL, &new);
      ReportInternalError("Out of memory.");
      return new;
    }

    // string literal is likely much longer than
    // identifier or any other buffered lexeme
    // try to shrink it
    if (!CharBufTryShrink(lexer->buf)) {
      TokenSetError(ERROR_INTERNAL, &new);
      ReportInternalError("Out of memory.");
      return new;
    }
    break;

  case STATE_NEWLINE:
    new.type = TOKEN_NEWLINE;
    new.indent = MeasureIndent(lexer);
    break;

  case STATE_ERROR:

    new.type = TOKEN_ERROR;

    ReportLexicalError(
        MSG_CONTEXT_T(.lineNum = new.pos.line, .hlStartCol = new.pos.start,
                      .hlEndCol = new.pos.end, .poiCol = new.pos.end),
        CharBufGetBuf(lexer->buf), "Unexpected character sequence.", 1);

    break;

  default:
    new.type = TOKEN_ERROR;

    ReportInternalError("Unknown lexer state.");
    break;
  }

  new.pos.end = lexer->col; // set end

  return new;
}

// ##############################################################
//                    LEXER.H IMPLEMENTATION
// ##############################################################

/** initialize a lexer struct
 * lexer - pointer to a lexer struct
 * src - pointer to buffer to scan
 */
Lexer_t *LexerCreate(FILE *src) {
  assert(src != NULL);

  Lexer_t *lexer = malloc(sizeof(*lexer));
  if (!lexer) {
    return NULL;
  }

  lexer->src = src;
  lexer->buf = CharBufCreate();
  if (!lexer->buf) {
    return NULL;
  }
  lexer->offset = 0;
  lexer->line = 1; // first line
  lexer->col = 1;  // col 0

  // initialize the map and transition table
  ASCIIMapInit();
  TransitionTableInit();

  // store line 1 offset
  MessengerStoreNewLine(ftell(src));

  lexer->prevChar = 0;

  return lexer;
}

char *LexerGetBuf(Lexer_t *lexer) {
  if (!lexer) {
    return NULL;
  }
  return CharBufGetBuf(lexer->buf);
}
/* terminate a lexer */
void LexerTerminate(Lexer_t *lexer) {
  if (!lexer) {
    return;
  }

  lexer->src = NULL;
  CharBufTerminate(lexer->buf);
  free(lexer);
}
