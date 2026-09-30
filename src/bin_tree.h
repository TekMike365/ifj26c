/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#ifndef BIN_TREE_H
#define BIN_TREE_H

typedef enum Color {
    BLACK,
    RED
} Color;

typedef struct Node {
    int value;
    Color color;
    struct Node *left;
    struct Node *right;
    struct Node *parent;
} Node;

typedef enum Direction {
    LEFT,
    RIGHT
} Direction;

Node *append(Node *root, int value);
Node *deleteNode(Node *root, int value);
void rotate(Node **root, Node *node, Direction direction);
void draw(Node *root);

#endif