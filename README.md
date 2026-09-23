# Git

## Branches

* master branch is sacred
* master branch is not to be altered without a review of at least 1 other member
* you can add to master only via reviewed PRs
* other branches are to be named `<xlogin-name>`, you then also become the
  maintainer of such branch (let's call them `xbranches`)
* if you want to contribute to `xbranch` you should branch off of it, create
  a new branch and then make a PR

## Commits

Commits are based on [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/#summary)

```
<type>[optional scope]: <description>

[optional body]

[optional footer]
```

 * Can use other types than recommended

# Code Style

 * This header is required to be at the start of every source file.

```c
/* Implementace překladače imperativního jazyka IFJ26.
 * Author[s]: Full Name <xlogin>
 *            Full Name <xlogin>
 */
```

 * Code is formatted via `clang-format` using the format specified in
   `.clang-format` file. (There's a `make format` target which will format
   every `*.c` or `*.h` file in the codebase using `clang-format`.)

## Naming conventions

```c
#define NICE_NUMBER 69
#define MAKE_NICE(x) x##69

/* Comment */
// Or Comment

typedef int NewType_t;

int g_globalVariable;
static int m_memberVariable;

enum { ENUM_VALUE, };

void SomeFunction(void) {
  static int s_staticVariable;
  int variableName;
}

/* if for some reason we need a namespace */
void Namespace_SomeNamespaceFunction(void);
```

When in doubt use `cammelCase`.
