/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Richard Blšták <xblstar00>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bin_tree.h"

static void insertFixup(Node **root, Node *node);
static void deleteFixup(Node **root, Node *node, Node *parent, int nodeIsLeft);
static void drawNode(Node *node, const char *prefix, size_t prefixLength,
                     int isLast, int isRoot);

Node *append(Node *root, int value)
{
    Node *parent = NULL;
    Node *current = root;

    while (current != NULL) {
        parent = current;
        if (value < current->value) {
            current = current->left;
        } else {
            current = current->right;
        }
    }

    Node *node = malloc(sizeof *node);
    if (node == NULL) {
        return root;
    }

    node->value = value;
    node->color = parent == NULL ? BLACK : RED;
    node->left = NULL;
    node->right = NULL;
    node->parent = parent;

    if (parent == NULL) {
        return node;
    }

    if (value < parent->value) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    insertFixup(&root, node);
    return root;
}

static void transplant(Node **root, Node *oldNode, Node *newNode)
{
    if (oldNode->parent == NULL) {
        *root = newNode;
    } else if (oldNode == oldNode->parent->left) {
        oldNode->parent->left = newNode;
    } else {
        oldNode->parent->right = newNode;
    }

    if (newNode != NULL) {
        newNode->parent = oldNode->parent;
    }
}

static Node *minimumNode(Node *node)
{
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

Node *deleteNode(Node *root, int value)
{
    Node *node = root;
    while (node != NULL && node->value != value) {
        node = value < node->value ? node->left : node->right;
    }
    if (node == NULL) {
        return root;
    }

    Node *replacement;
    Node *replacementParent;
    int replacementIsLeft;
    Color removedColor;

    if (node->left == NULL) {
        replacement = node->right;
        replacementParent = node->parent;
        replacementIsLeft = replacementParent != NULL && node == replacementParent->left;
        removedColor = node->color;
        transplant(&root, node, replacement);
    } else if (node->right == NULL) {
        replacement = node->left;
        replacementParent = node->parent;
        replacementIsLeft = replacementParent != NULL && node == replacementParent->left;
        removedColor = node->color;
        transplant(&root, node, replacement);
    } else {
        Node *successor = minimumNode(node->right);
        removedColor = successor->color;
        replacement = successor->right;
        if (successor->parent != node) {
            replacementParent = successor->parent;
            replacementIsLeft = successor == replacementParent->left;
            transplant(&root, successor, successor->right);
            successor->right = node->right;
            successor->right->parent = successor;
        } else {
            replacementParent = successor;
            replacementIsLeft = 0;
            if (replacement != NULL) {
                replacement->parent = successor;
            }
        }
        transplant(&root, node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
        successor->color = node->color;
    }

    free(node);
    if (removedColor == BLACK) {
        deleteFixup(&root, replacement, replacementParent, replacementIsLeft);
    }
    if (root != NULL) {
        root->color = BLACK;
    }
    return root;
}

void rotate(Node **root, Node *node, Direction direction)
{
    if (root == NULL || node == NULL) {
        return;
    }

    Node *pivot;
    Node **nodeChild;
    Node **pivotChild;
    if (direction == LEFT) {
        pivot = node->right;
        if (pivot == NULL) {
            return;
        }
        nodeChild = &node->right;
        pivotChild = &pivot->left;
    } else if (direction == RIGHT) {
        pivot = node->left;
        if (pivot == NULL) {
            return;
        }
        nodeChild = &node->left;
        pivotChild = &pivot->right;
    } else {
        return;
    }

    *nodeChild = *pivotChild;
    if (*nodeChild != NULL) {
        (*nodeChild)->parent = node;
    }

    pivot->parent = node->parent;
    if (node->parent == NULL) {
        *root = pivot;
    } else if (node == node->parent->left) {
        node->parent->left = pivot;
    } else {
        node->parent->right = pivot;
    }

    *pivotChild = node;
    node->parent = pivot;
}

void draw(Node *root)
{
    if (root == NULL) {
        puts("(empty tree)");
        return;
    }

    drawNode(root, "", 0, 1, 1);
}

static void drawNode(Node *node, const char *prefix, size_t prefixLength,
                     int isLast, int isRoot)
{
    const char *color = node->color == RED ? "RED" : "BLACK";
    if (isRoot) {
        printf("%d [%s]\n", node->value, color);
    } else {
        printf("%s%s%d [%s]\n", prefix, isLast ? "\\-- " : "|-- ",
               node->value, color);
    }

    size_t childPrefixLength = prefixLength;
    char childPrefix[prefixLength + 5];
    if (prefixLength > 0) {
        memcpy(childPrefix, prefix, prefixLength);
    }
    if (isRoot) {
        childPrefix[0] = '\0';
        childPrefixLength = 0;
    } else {
        memcpy(childPrefix + prefixLength, isLast ? "    " : "|   ", 5);
        childPrefixLength += 4;
    }

    if (node->left != NULL) {
        drawNode(node->left, childPrefix, childPrefixLength,
                 node->right == NULL, 0);
    }
    if (node->right != NULL) {
        drawNode(node->right, childPrefix, childPrefixLength, 1, 0);
    }
}

static Color colorOf(Node *node)
{
    return node == NULL ? BLACK : node->color;
}

static void insertFixup(Node **root, Node *node)
{
    while (node != *root && node->parent->color == RED) {
        Node *parent = node->parent;
        Node *grandparent = parent->parent;

        if (parent == grandparent->left) {
            Node *uncle = grandparent->right;
            if (colorOf(uncle) == RED) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                node = grandparent;
            } else {
                if (node == parent->right) {
                    node = parent;
                    rotate(root, node, LEFT);
                }
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                rotate(root, node->parent->parent, RIGHT);
            }
        } else {
            Node *uncle = grandparent->left;
            if (colorOf(uncle) == RED) {
                parent->color = BLACK;
                uncle->color = BLACK;
                grandparent->color = RED;
                node = grandparent;
            } else {
                if (node == parent->left) {
                    node = parent;
                    rotate(root, node, RIGHT);
                }
                node->parent->color = BLACK;
                node->parent->parent->color = RED;
                rotate(root, node->parent->parent, LEFT);
            }
        }
    }

    (*root)->color = BLACK;
}

static void deleteFixup(Node **root, Node *node, Node *parent, int nodeIsLeft)
{
    while (node != *root && colorOf(node) == BLACK) {
        if (parent == NULL) {
            break;
        }
        if (node != NULL) {
            nodeIsLeft = node == parent->left;
        }

        if (nodeIsLeft) {
            Node *sibling = parent->right;
            if (colorOf(sibling) == RED) {
                sibling->color = BLACK;
                parent->color = RED;
                rotate(root, parent, LEFT);
                sibling = parent->right;
            }

            if (colorOf(sibling == NULL ? NULL : sibling->left) == BLACK &&
                colorOf(sibling == NULL ? NULL : sibling->right) == BLACK) {
                if (sibling != NULL) {
                    sibling->color = RED;
                }
                node = parent;
                parent = node->parent;
            } else {
                if (colorOf(sibling->right) == BLACK) {
                    sibling->left->color = BLACK;
                    sibling->color = RED;
                    rotate(root, sibling, RIGHT);
                    sibling = parent->right;
                }
                sibling->color = parent->color;
                parent->color = BLACK;
                if (sibling->right != NULL) {
                    sibling->right->color = BLACK;
                }
                rotate(root, parent, LEFT);
                node = *root;
                parent = NULL;
            }
        } else {
            Node *sibling = parent->left;
            if (colorOf(sibling) == RED) {
                sibling->color = BLACK;
                parent->color = RED;
                rotate(root, parent, RIGHT);
                sibling = parent->left;
            }

            if (colorOf(sibling == NULL ? NULL : sibling->right) == BLACK &&
                colorOf(sibling == NULL ? NULL : sibling->left) == BLACK) {
                if (sibling != NULL) {
                    sibling->color = RED;
                }
                node = parent;
                parent = node->parent;
            } else {
                if (colorOf(sibling->left) == BLACK) {
                    sibling->right->color = BLACK;
                    sibling->color = RED;
                    rotate(root, sibling, LEFT);
                    sibling = parent->left;
                }
                sibling->color = parent->color;
                parent->color = BLACK;
                if (sibling->left != NULL) {
                    sibling->left->color = BLACK;
                }
                rotate(root, parent, RIGHT);
                node = *root;
                parent = NULL;
            }
        }
    }

    if (node != NULL) {
        node->color = BLACK;
    }
}

static void showAppend(Node **root, int value)
{
    printf("\nAppend %d\nBefore:\n", value);
    draw(*root);
    *root = append(*root, value);
    puts("After:");
    draw(*root);
}

static void showDelete(Node **root, int value)
{
    printf("\nDelete %d\nBefore:\n", value);
    draw(*root);
    *root = deleteNode(*root, value);
    puts("After:");
    draw(*root);
}

static void freeTree(Node *root)
{
    if (root == NULL) {
        return;
    }
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}

