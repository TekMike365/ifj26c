/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Ondrej Dedič <xdedico00>
 */

#include "parser_internal.h"

/* allocates memory and duplicates string */
char *StrDup(const char *s) {
  if (s == NULL)
    return NULL;
  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if (copy != NULL) {
    memcpy(copy, s, len);
  }
  return copy;
}

/* loads new token from preprocesor
 * If new p->current is ERROR_TOKEN, sets error code from TOKEN
 * returns true if no error, else false
 */
bool Advance(Parser_t *p) {
  p->last = p->current;
  p->current = p->next;
  if (p->current.pos.line > 0 && p->current.type != TOKEN_NEWLINE)
    p->lastValidPos = p->current.pos;

  if (p->current.type == TOKEN_ERROR) {
    p->errorCode = p->current.error;
    return false;
  }

  p->next = PPNextToken(p->pp);

  return true;
}

/* reports syntax error with detail */
void SyntaxError(Parser_t *p, const char *detail) {
  if (p->errorCode == ERROR_NOERR)
    p->errorCode = ERROR_SYNTAX;

  SourceSpan_t pos = p->lastValidPos;

  MsgContext_t ctx = MSG_CONTEXT_T(.lineNum = pos.line, .hlStartCol = pos.start,
                                   .hlEndCol = pos.end, .poiCol = pos.start);

  ReportSyntaxError(ctx, "Syntax Error", detail, pos.line > 0);
}

/* returns token type in string format, for error handling */
const char *TokenToString(TokenType_t type) {
  switch (type) {
  case TOKEN_NONE:
    return "none";
  case TOKEN_IDENTIFIER:
    return "identifier";
  case TOKEN_INDENT:
    return "indent";
  case TOKEN_DEDENT:
    return "dedent";
  case TOKEN_KEYWORD_DO:
    return "'do'";
  case TOKEN_KEYWORD_IF:
    return "'if'";
  case TOKEN_KEYWORD_DEF:
    return "'def'";
  case TOKEN_KEYWORD_FOR:
    return "'for'";
  case TOKEN_KEYWORD_INT:
    return "'Int'";
  case TOKEN_KEYWORD_VAL:
    return "'val'";
  case TOKEN_KEYWORD_VAR:
    return "'var'";
  case TOKEN_KEYWORD_CASE:
    return "'case'";
  case TOKEN_KEYWORD_ELSE:
    return "'else'";
  case TOKEN_KEYWORD_LIST:
    return "'List'";
  case TOKEN_KEYWORD_THEN:
    return "'then'";
  case TOKEN_KEYWORD_TYPE:
    return "'type'";
  case TOKEN_KEYWORD_UNIT:
    return "'Unit'";
  case TOKEN_KEYWORD_CLASS:
    return "'class'";
  case TOKEN_KEYWORD_MATCH:
    return "'match'";
  case TOKEN_KEYWORD_WHILE:
    return "'while'";
  case TOKEN_KEYWORD_DOUBLE:
    return "'Double'";
  case TOKEN_KEYWORD_IMPORT:
    return "'import'";
  case TOKEN_KEYWORD_RETURN:
    return "'return'";
  case TOKEN_KEYWORD_STRING:
    return "'String'";
  case TOKEN_KEYWORD_BOOLEAN:
    return "'Boolean'";
  case TOKEN_KEYWORD_EXTENSION:
    return "'extension'";
  case TOKEN_EQ:
    return "'='";
  case TOKEN_EXPECT_EOL:
    return "end of line";
  case TOKEN_NOT_EQ:
    return "'!='";
  case TOKEN_DOUBLE_EQ:
    return "'=='";
  case TOKEN_GREATER:
    return "'>'";
  case TOKEN_GREATER_EQ:
    return "'>='";
  case TOKEN_LESS:
    return "'<'";
  case TOKEN_LESS_EQ:
    return "'<='";
  case TOKEN_LEFT_ARROW:
    return "'<-'";
  case TOKEN_STAR:
    return "'*'";
  case TOKEN_PLUS:
    return "'+'";
  case TOKEN_MINUS:
    return "'-'";
  case TOKEN_SLASH:
    return "'/'";
  case TOKEN_SEMICOLON:
    return "';'";
  case TOKEN_COMMA:
    return "','";
  case TOKEN_COLON:
    return "':'";
  case TOKEN_ZERO:
    return "'0'";
  case TOKEN_INT:
    return "integer literal";
  case TOKEN_DOUBLE:
    return "double literal";
  case TOKEN_STRING:
    return "string literal";
  case TOKEN_LPAR:
    return "'('";
  case TOKEN_RPAR:
    return "')'";
  case TOKEN_AT:
    return "'@'";
  case TOKEN_AMPERSAND:
    return "'&'";
  case TOKEN_PIPE:
    return "'|'";
  case TOKEN_AND:
    return "'&&'";
  case TOKEN_OR:
    return "'||'";
  case TOKEN_DOT:
    return "'.'";
  case TOKEN_UNIT:
    return "'()'";
  case TOKEN_EOF:
    return "end of file";
  case TOKEN_NEWLINE:
    return "newline";
  case TOKEN_ERROR:
    return "lexical error";
  case TOKEN_UNARY_PLUS:
    return "unary '+'";
  case TOKEN_UNARY_MINUS:
    return "unary '-'";
  default:
    return "unknown token";
  }
}