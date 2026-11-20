#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum {
    N_PROGRAM, N_FUNC, N_BLOCK, N_DECLS, N_DECL, N_EXPRSTMT, N_EMPTY,
    N_IF, N_WHILE, N_DOWHILE, N_FOR, N_RETURN, N_BREAK, N_CONTINUE, N_PRINT,
    N_NUM, N_STR, N_VAR, N_BIN, N_UN, N_ASSIGN, N_INCDEC, N_COND, N_CALL, N_LIST
} NodeKind;

typedef struct Sym {
    char *name;
    char *pyname;
    int global;
    int idx;
    struct Sym *next;
} Sym;

typedef struct Node {
    NodeKind kind;
    int line;
    char *text;
    long long val;
    int flag;
    Sym *sym;
    struct Node *func;
    struct Node **kids;
    int nkids;
    int cap;
    int nparams;
    int nlocals;
    int is_void;
    Sym *syms;
    Sym **local_syms;
} Node;

extern int error_count;

Node *mk(NodeKind kind, const char *text, ...);
Node *mk_num(long long v);
void add_kid(Node *parent, Node *kid);
const char *kind_name(NodeKind k);

void sem_error(int line, const char *fmt, ...);
void scope_push(void);
void scope_pop(void);
void func_begin(Node *fn);
void func_end(void);
Sym *declare_var(const char *name, int line);
Sym *lookup_var(const char *name);
void register_func(Node *fn);
Node *find_func(const char *name);
void resolve_calls(Node *program);
int global_count(void);

void dump_tree(Node *n, FILE *out);
void dump_json(Node *n, FILE *out);

int interpret(Node *program);
void gen_python(Node *program, FILE *out);

#endif
