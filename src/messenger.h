/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Michael Sklenka <xsklenm00>
 *
 * A unified messaging system.
 *
 * This module provides a unified status messaging system for the compiler. It
 * has the ability to print simple stand-alone mesages as well as messages with
 * added context. If possible the context line is printed with simple
 * decorations.
 *
 * Example message with all context printed:
 *
 * [INFO] Dis a message.
 *   In function `foo()'
 *   67:5: wrong, you're wrong here
 *   | int @foo;
 *   |     ^~~~
 */
#ifndef IFJ26C_MESSENGER_H
#define IFJ26C_MESSENGER_H

#include <stddef.h>

#define MAX_MSG_LENGTH 4095 /* excluding the NULL byte */

/* A helper macro for creating contexts. */
#define MSG_CONTEXT_T(...) ((MsgContext_t){__VA_ARGS__})

#define FOR_MESSAGE_TYPE(DO)                                                   \
  DO(TRACE)                                                                    \
  DO(INFO)                                                                     \
  DO(WARNING)                                                                  \
  DO(ERROR)

#define TO_ENUM(x) MSG_##x,
typedef enum { FOR_MESSAGE_TYPE(TO_ENUM) } MsgType_t;
#undef TO_ENUM

typedef enum {
  MSG_ERR_OK = 0,
  MSG_ERR_NORETURN,
  MSG_ERR_FAILED,
} MsgErr_t;

typedef struct message Message_t;

typedef struct {
  /* MsgContext_t
   *   `lineNum' and each `Col' field is indexed from 1, the value 0 is reserved
   * and means `empty'.
   */

  /* line to show*/
  size_t lineNum;

  /* highlighted region */
  size_t hlStartCol;
  size_t hlEndCol;

  /* point of interest */
  size_t poiCol;
} MsgContext_t;

void MessengerInit(void);      /* allocates resources */
void MessengerTerminate(void); /* frees up used resources */

/* MessengerStoreNewLine()
 *   If the file is seekable stores the file offset to a new line. Returns
 * the associated line number or -1 if an Error occurred.
 */
long MessengerStoreNewLine(long offset);

/* MsgStart()
 *   Starts a new message with the format `fmt' and returns its handle.
 * Formatting works the same as in sprintf(). If the formatted string exceeds
 * MAX_MSG_LENGTH, it gets truncated to fit.
 *
 *   On error returns NULL.
 */
Message_t *MsgStart(MsgType_t type, const char *fmt, ...);

/* MsgDrop()
 *   Drops `msg' and frees it's resources. Using `msg' after it was dropped will
 * result in undefined behavior.
 */
void MsgDrop(Message_t *msg);

/* MsgDrop()
 *   Sends `msg' to stderr and frees it's resources. Using `msg' after it was
 * sent will result in undefined behavior.
 *
 *   This function tries to read context from the input file. If it fails to
 * return to the original possition it returns error `MSG_ERR_NORETURN'. If such
 * error occures the file offset is pointed to a random place in the file, it
 * can be both before or after the original place.
 *
 * Returns:
 *   MSG_ERR_OK        when everything is fine, everything is OK, etc.
 *   MSG_ERR_NORETURN  when it failed to return to the original position.
 *   MSG_ERR_FAILED    when `msg' is NULL
 */
MsgErr_t MsgSend(Message_t *msg);

/* MsgAddContext()
 *   Adds a new context `ctx' to `msg' with the formatted message `fmt'.
 * Formatting works the same as in sprintf(). If the formatted string exceeds
 * MAX_MSG_LENGTH, it gets truncated to fit.
 */
void MsgAddContext(Message_t *msg, MsgContext_t ctx, const char *fmt, ...);

#endif /* IFJ26C_MESSENGER_H */
