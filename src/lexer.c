/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Dominik Makuka <xmakukd00>
 */

#include "lexer.h"
#include "char_buf.h"
#include "int_stack.h"
#include "lexer_fsm.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/**TODO:
 * START:WHITESPACE -> START?
 *
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
 * */
static inline TokenType_t MatchKeywordSuffix(const char *str, const char *ref,
                                             size_t len, TokenType_t matched) {

  if (memcmp(str, ref, len) == 0) {
    return matched;
  }

  return TOKEN_IDENTIFIER;
}

/* decides whether str is identifier or keyword
 * implemented using length-partition followed by trie matching
 * str - pointer to a char array
 * return - TOKEN_NONE on str == NULL, TOKEN_KEYWORD_X on keyword X, else
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
    case 'd':
      // match the rest with "o" as in "do"
      //                    rest has len 1 ---v
      return MatchKeywordSuffix(str + 1, "o", 1, TOKEN_KEYWORD_DO); // 'do'
    case 'i':
      return MatchKeywordSuffix(str + 1, "f", 1, TOKEN_KEYWORD_IF); // 'if'
    }
    break;
  case 3:
    switch (str[0]) {
    case 'd':
      return MatchKeywordSuffix(str + 1, "ef", 2, TOKEN_KEYWORD_DEF); // 'def'
    case 'f':
      return MatchKeywordSuffix(str + 1, "or", 2, TOKEN_KEYWORD_FOR); // 'for'
    case 'I':
      return MatchKeywordSuffix(str + 1, "nt", 2, TOKEN_KEYWORD_INT); // 'Int'
    case 'v':
      switch (str[1]) {
        // annoying, both ('var'/'val') start as 'va' and have length 3 :(
        // kept switch for consistency
      case 'a':
        switch (str[2]) {
        case 'l':
          return TOKEN_KEYWORD_VAL; // 'val'

        case 'r':
          return TOKEN_KEYWORD_VAR; // 'var'
        }
      }
    }
    break;

  case 4:
    switch (str[0]) {
    case 'c':
      return MatchKeywordSuffix(str + 1, "ase", 3,
                                TOKEN_KEYWORD_CASE); // 'case'
    case 'e':
      return MatchKeywordSuffix(str + 1, "lse", 3,
                                TOKEN_KEYWORD_ELSE); // 'else'
    case 'L':
      return MatchKeywordSuffix(str + 1, "ist", 3,
                                TOKEN_KEYWORD_LIST); // 'List'
    case 't':
      switch (str[1]) {
      case 'h':
        return MatchKeywordSuffix(str + 2, "en", 2,
                                  TOKEN_KEYWORD_THEN); // 'then'
      case 'y':
        return MatchKeywordSuffix(str + 2, "pe", 2,
                                  TOKEN_KEYWORD_TYPE); // 'type'

      default:
        return TOKEN_IDENTIFIER;
      }
    case 'U':
      return MatchKeywordSuffix(str + 1, "nit", 3,
                                TOKEN_KEYWORD_UNIT); // 'Unit'
    }
    break;

  case 5:
    switch (str[0]) {
    case 'c':
      return MatchKeywordSuffix(str + 1, "lass", 4,
                                TOKEN_KEYWORD_CLASS); // 'class'
    case 'm':
      return MatchKeywordSuffix(str + 1, "atch", 4,
                                TOKEN_KEYWORD_MATCH); // 'match'
    case 'w':
      return MatchKeywordSuffix(str + 1, "hile", 4,
                                TOKEN_KEYWORD_WHILE); // 'while'
    }
    break;
  case 6:
    switch (str[0]) {
    case 'D':
      return MatchKeywordSuffix(str + 1, "ouble", 5,
                                TOKEN_KEYWORD_DOUBLE); // 'Double'
    case 'i':
      return MatchKeywordSuffix(str + 1, "mport", 5,
                                TOKEN_KEYWORD_IMPORT); // 'import'
    case 'r':
      return MatchKeywordSuffix(str + 1, "eturn", 5,
                                TOKEN_KEYWORD_RETURN); // 'return'
    case 'S':
      return MatchKeywordSuffix(str + 1, "tring", 5,
                                TOKEN_KEYWORD_STRING); // 'String'
    }
    break;
  case 7:
    return MatchKeywordSuffix(str + 0, "Boolean", 7,
                              TOKEN_KEYWORD_BOOLEAN); // 'Boolean'
    // no keyword with length 8
  case 8:
    return TOKEN_IDENTIFIER;
  case 9:
    return MatchKeywordSuffix(str + 0, "extension", 9,
                              TOKEN_KEYWORD_EXTENSION); // 'extension'

  default:
    return TOKEN_IDENTIFIER;
  }
  return TOKEN_IDENTIFIER;
}

char *InStringpool(char *str) {
  (void)str;
  return NULL;
}

static inline int GetNextChar(Lexer_t *lexer) {
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
  }
  // if not EOF, increment offset
  if (c != EOF)
    lexer->offset++;

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
 * else if multiple, replaced by newline
 * if last line continues with non-whitespace, error
 * */
static _Bool SkipBlockComment(Lexer_t *lexer) {
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

  // single line block comment, replace by space and return
  if (!hasNewline) {
    lexer->prevChar = ' ';
    return (depth == 0);
  }

  // else read to the end of last line
  while ((c = GetNextChar(lexer)) != '\n' && c != EOF) {
    // non-whistespace symbol on the last line - error
    if (!isspace(c)) {
      return 0;
      // TODO: handle // comment begin, not an error
      // dont set \n on EOF???
    }
  }

  // replace by '\n'
  lexer->prevChar = '\n';

  // return: 1 => skipped without problem; 0 => EOF before comment end or last
  // line tokens - error
  return (depth == 0);
}

static void SkipLineComment(Lexer_t *lexer) {
  int c;
  // read until EOF or line feed
  while ((c = GetNextChar(lexer)) != EOF && c != '\n') {
    continue;
  }
  // return c to "stream"
  lexer->prevChar = c;
}

static void MeasureIndent(Lexer_t *lexer) {
  int indentLevel = 0;
  int c;

  while ((c = GetNextChar(lexer)) == ' ') {
    indentLevel++;
  }
}

/* Function tries to put 'indent' on top of the indent stack
 * * indent - incoming indent to validate
 * * indentStack - stack of active indents
 * * levelsChanged - out parameter, count of in/dedent
 * * returns 1 on sucess, else 0
 * if 'indent' is greater than top, indent increased
 * if 'indent' is smaller, it is a dedent -> pop until matching indent is found
 * if not found, there was no matching indent -> error
 * */
/*static _Bool UpdateIndentStack(int indent, Lexer_t *lexer) {
  int change = 0; // level change, zero intially

  // is empty or top is smaller -> indent, push
  if (IntStackIsEmpty(&lexer->indentStack) ||
      IntStackTop(&lexer->indentStack) < indent) {
    IntStackPush(&lexer->indentStack, indent);
    change++; // indent increased
  }
  // else dedent -> try to put it on top of the stack
  // pop until indent is smaller than top and stack is not empty
  else {
    while (!IntStackIsEmpty(&lexer->indentStack) &&
           IntStackTop(&lexer->indentStack) > indent) {
      IntStackPop(&lexer->indentStack);
      change--; // indent decreased
    }
  }

  // if the stack is empty or indent not on top
  // error, there was no indent to match the dedent
  if (IntStackIsEmpty(&lexer->indentStack) ||
      IntStackTop(&lexer->indentStack) != indent) {
    return 0;
  }

  // assign change
  lexer->indentChange = change;

  // success
  return 1;
}*/

/*static inline void DedentOnEOF(Lexer_t *lexer) {
  assert(lexer != NULL);
  lexer->indentChange = IntStackPopAll(&lexer->indentStack);
}*/

// ##############################################################
//                    LEXER.H IMPLEMENTATION
// ##############################################################

/** initialize a lexer struct
 * lexer - pointer to a lexer struct
 * src - pointer to buffer to scan
 */
void LexerInit(Lexer_t *lexer, FILE *src) {
  assert(lexer != NULL);

  lexer->src = src;
  CharBufInit(&lexer->buf);
  lexer->offset = 0;
  lexer->line = 1; // first line

  // initialize the map and transition table
  ASCIIMapInit();
  TransitionTableInit();

  lexer->prevChar = 0;
}

void LexerTerminate(Lexer_t *lexer) {
  if (lexer == NULL) {
    return;
  }

  lexer->src = NULL;
  CharBufTerminate(&lexer->buf);
  lexer->prevChar = 0;
  lexer->line = 1;
  lexer->offset = 0;
}

_Bool LayoutFilterInit(LayoutFilter_t *filter, FILE *src) {
  LexerInit(&filter->lexer, src);
  IntStackInit(&filter->indents);
  if (!IntStackPush(&filter->indents, 0)) {
    return 0;
  }
  filter->stash = (Token_t){0};
  filter->hasStash = 0;

  filter->atLineStart = 1;
  filter->pendingDedents = 0;

  filter->nextIndent = 0;

  filter->bracketDepth = 0;
  return 1;
}

void LayoutFilterTerminate(LayoutFilter_t *filter) {
  LexerTerminate(&filter->lexer);
  IntStackTerminate(&filter->indents);
  filter->stash = (Token_t){0};
  filter->hasStash = 0;

  filter->atLineStart = 1;
  filter->pendingDedents = 0;

  filter->nextIndent = 0;

  filter->bracketDepth = 0;
}

Token_t CreateToken(void) {
  Token_t new = {.lexemeS = NULL, .line = 0, .len = 0, .type = TOKEN_NONE};
  return new;
}

/* TODO
 * indent
 * tryshrink after string - probably longer than identifier
 * string pool
 */
Token_t GetRawToken(Lexer_t *lexer) {

  // create new token
  Token_t new = CreateToken();

  // on EOF, return TOKEN_EOF
  if (lexer->prevChar == EOF) {
    new.type = TOKEN_EOF;
    return new;
  }

  State_t current, next;

RESET:

  // reset buffer
  CharBufClear(&lexer->buf);

  current = STATE_START;

  int c;
  while ((c = GetNextChar(lexer)) != EOF) {

    CharType_t type = MapChar(c);

    next = NextState(current, type);
    if (next == STATE_END) {
      break;
    }

    // TODO: not ERROR, but error code
    if (!CharBufInsert(&lexer->buf, c)) {
      new.type = TOKEN_ERROR;
      return new;
    };

    current = next;
  }

  // null-terminate
  if (!CharBufInsert(&lexer->buf, '\0')) {
    new.type = TOKEN_ERROR;
    return new;
  };

  // next char was consumed to determine end
  // i.e. '+' in "pos+1" is a delimiter for "pos"
  // must push it back into "stream"
  // prevChar replaces ungetc
  lexer->prevChar = c;

  // new.lexemeS = InStringpool()

  // can be optimized by STATE->TOKEN_TYPE map
  switch (current) {

  case STATE_START:
    new.type = TOKEN_EOF;
    break;

  case STATE_IDENTIFIER:
    // IF STRING POOL CONTAINED STRING+METADATA i.e. TOKEN POOL
    // Token type could be a part of it?
    // Parse not needed?
    new.type = ParseIdentifier(lexer->buf.buf);
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

  case STATE_COMMENT:
    SkipLineComment(lexer);
    // comment not a token, start anew
    goto RESET;

  case STATE_BLOCK_COMMENT:
    if (!SkipBlockComment(lexer)) {
      new.type = TOKEN_ERROR;
      break;
    }
    // start anew
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

  case STATE_UNIT:
    new.type = TOKEN_UNIT;
    break;

  case STATE_INT:
    new.type = TOKEN_INT;
    new.lexemeI = StrToInt(lexer->buf.buf, NULL);
    break;

  case STATE_DOUBLE:
  case STATE_EXPONENT:
  case STATE_END_DOUBLE:
    new.type = TOKEN_DOUBLE;
    new.lexemeD = StrToDouble(lexer->buf.buf, NULL);
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

  case STATE_END_STRING:
  case STATE_END_MULTILINE_STR:
    new.type = TOKEN_STRING;
    break;

  case STATE_NEWLINE:
    new.type = TOKEN_NEWLINE;
    break;

  default:
    new.type = TOKEN_ERROR;
    break;
  }

  return new;
}

// dummy for now
Token_t GetNextToken(Lexer_t *lexer) { return GetRawToken(lexer); }
