#define _GNU_SOURCE
#include "ast.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int error_count = 0;

static const char *kind_names[] = {
    "Program", "Function", "Block", "Declarations", "Declaration", "ExprStmt", "Empty",
    "If", "While", "DoWhile", "For", "Return", "Break", "Continue", "Print",
    "Number", "String", "Variable", "BinaryOp", "UnaryOp", "Assign", "IncDec", "Ternary", "Call", "List"
};

const char *kind_name(NodeKind k)
{
    return kind_names[k];
}

void add_kid(Node *p, Node *k)
{
    if (p->nkids == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 4;
        p->kids = realloc(p->kids, p->cap * sizeof(Node *));
    }
    p->kids[p->nkids++] = k;
}

extern int yylineno;

Node *mk(NodeKind kind, const char *text, ...)
{
    Node *n = calloc(1, sizeof(Node));
    n->kind = kind;
    n->line = yylineno;
    n->text = text ? strdup(text) : NULL;
    va_list ap;
    va_start(ap, text);
    Node *k;
    while ((k = va_arg(ap, Node *)) != NULL)
        add_kid(n, k);
    va_end(ap);
    return n;
}

Node *mk_num(long long v)
{
    Node *n = mk(N_NUM, NULL, NULL);
    n->val = v;
    return n;
}

void sem_error(int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "line %d: error: ", line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    error_count++;
}

#define MAX_SCOPES 256

static Sym *scopes[MAX_SCOPES];
static int scope_top = 0;
static Node *cur_func = NULL;
static int globals = 0;

int global_count(void)
{
    return globals;
}

void scope_push(void)
{
    scope_top++;
    scopes[scope_top] = NULL;
}

void scope_pop(void)
{
    scope_top--;
}

void func_begin(Node *fn)
{
    cur_func = fn;
    scope_push();
}

void func_end(void)
{
    scope_pop();
    cur_func = NULL;
}

static const char *reserved_py[] = {
    "int", "abs", "print", "not", "and", "or", "if", "else", "elif", "while", "for", "in", "is", "def",
    "class", "pass", "None", "True", "False", "lambda", "global", "import", "from", "as", "with",
    "try", "except", "finally", "raise", "yield", "del", "assert", "return", "break", "continue",
    "sys", "_div", "_mod", "len", "str", "list", "range", "min", "max", "sum", "type", "id", NULL
};

static int is_reserved(const char *s)
{
    for (int i = 0; reserved_py[i]; i++)
        if (strcmp(reserved_py[i], s) == 0)
            return 1;
    return 0;
}

static char *make_pyname(const char *name, int global, int same_count)
{
    char buf[256];
    const char *base = name;
    char tmp[200];
    if (is_reserved(name)) {
        snprintf(tmp, sizeof tmp, "%s_", name);
        base = tmp;
    }
    if (global)
        snprintf(buf, sizeof buf, "%s", base);
    else if (same_count == 0)
        snprintf(buf, sizeof buf, "%s", base);
    else
        snprintf(buf, sizeof buf, "%s_%d", base, same_count);
    return strdup(buf);
}

Sym *declare_var(const char *name, int line)
{
    for (Sym *s = scopes[scope_top]; s; s = s->next)
        if (strcmp(s->name, name) == 0) {
            sem_error(line, "redeclaration of '%s'", name);
            return s;
        }
    Sym *s = calloc(1, sizeof(Sym));
    s->name = strdup(name);
    s->next = scopes[scope_top];
    scopes[scope_top] = s;
    if (cur_func == NULL) {
        s->global = 1;
        s->idx = globals++;
        s->pyname = make_pyname(name, 1, 0);
    } else {
        s->global = 0;
        s->idx = cur_func->nlocals++;
        int same = 0;
        for (Sym *o = cur_func->syms; o; o = o->next)
            if (strcmp(o->name, name) == 0)
                same++;
        s->pyname = make_pyname(name, 0, same);
        Sym *rec = calloc(1, sizeof(Sym));
        rec->name = s->name;
        rec->next = cur_func->syms;
        cur_func->syms = rec;
        cur_func->local_syms = realloc(cur_func->local_syms, cur_func->nlocals * sizeof(Sym *));
        cur_func->local_syms[s->idx] = s;
    }
    return s;
}

Sym *lookup_var(const char *name)
{
    for (int i = scope_top; i >= 0; i--)
        for (Sym *s = scopes[i]; s; s = s->next)
            if (strcmp(s->name, name) == 0)
                return s;
    return NULL;
}

#define MAX_FUNCS 256
static Node *funcs[MAX_FUNCS];
static int nfuncs = 0;

void register_func(Node *fn)
{
    if (find_func(fn->text)) {
        sem_error(fn->line, "redefinition of function '%s'", fn->text);
        return;
    }
    if (nfuncs < MAX_FUNCS)
        funcs[nfuncs++] = fn;
}

Node *find_func(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i]->text, name) == 0)
            return funcs[i];
    return NULL;
}

static void resolve_walk(Node *n)
{
    if (!n)
        return;
    if (n->kind == N_CALL) {
        Node *f = find_func(n->text);
        if (!f)
            sem_error(n->line, "call to undefined function '%s'", n->text);
        else if (f->nparams != n->nkids)
            sem_error(n->line, "function '%s' expects %d argument(s), got %d", n->text, f->nparams, n->nkids);
        n->func = f;
    }
    for (int i = 0; i < n->nkids; i++)
        resolve_walk(n->kids[i]);
}

void resolve_calls(Node *program)
{
    resolve_walk(program);
    if (!find_func("main"))
        sem_error(0, "function 'main' is not defined");
}

static void node_label(Node *n, char *buf, size_t size)
{
    switch (n->kind) {
    case N_NUM:
        snprintf(buf, size, "%lld", n->val);
        break;
    case N_STR:
        snprintf(buf, size, "\"%s\"", n->text);
        break;
    case N_VAR:
        snprintf(buf, size, "%s", n->text);
        break;
    case N_FUNC:
        snprintf(buf, size, "%s %s(%d)", n->is_void ? "void" : "int", n->text, n->nparams);
        break;
    case N_DECL:
        snprintf(buf, size, "int %s", n->text);
        break;
    case N_BIN:
    case N_UN:
        snprintf(buf, size, "%s", n->text);
        break;
    case N_ASSIGN:
        snprintf(buf, size, "%s", n->text);
        break;
    case N_INCDEC:
        snprintf(buf, size, "%s%s", n->flag ? n->text : "x", n->flag ? "x" : n->text);
        break;
    case N_CALL:
        snprintf(buf, size, "%s()", n->text);
        break;
    case N_COND:
        snprintf(buf, size, "?:");
        break;
    default:
        snprintf(buf, size, "%s", kind_name(n->kind));
    }
}

static void dump_rec(Node *n, FILE *out, int depth)
{
    char label[300];
    node_label(n, label, sizeof label);
    for (int i = 0; i < depth; i++)
        fputs("  ", out);
    if (n->kind == N_NUM || n->kind == N_VAR || n->kind == N_STR || n->kind == N_BIN || n->kind == N_UN ||
        n->kind == N_ASSIGN || n->kind == N_INCDEC || n->kind == N_CALL || n->kind == N_FUNC || n->kind == N_DECL)
        fprintf(out, "%s: %s\n", kind_name(n->kind), label);
    else
        fprintf(out, "%s\n", label);
    for (int i = 0; i < n->nkids; i++)
        dump_rec(n->kids[i], out, depth + 1);
}

void dump_tree(Node *n, FILE *out)
{
    dump_rec(n, out, 0);
}

static void json_str(const char *s, FILE *out)
{
    fputc('"', out);
    for (; *s; s++) {
        unsigned char c = *s;
        if (c == '"' || c == '\\')
            fprintf(out, "\\%c", c);
        else if (c == '\n')
            fputs("\\n", out);
        else if (c == '\t')
            fputs("\\t", out);
        else if (c < 32)
            fprintf(out, "\\u%04x", c);
        else
            fputc(c, out);
    }
    fputc('"', out);
}

static void json_rec(Node *n, FILE *out)
{
    char label[300];
    node_label(n, label, sizeof label);
    fputs("{\"kind\":", out);
    json_str(kind_name(n->kind), out);
    fputs(",\"label\":", out);
    json_str(label, out);
    fprintf(out, ",\"line\":%d", n->line);
    if (n->nkids) {
        fputs(",\"children\":[", out);
        for (int i = 0; i < n->nkids; i++) {
            if (i)
                fputc(',', out);
            json_rec(n->kids[i], out);
        }
        fputc(']', out);
    }
    fputc('}', out);
}

void dump_json(Node *n, FILE *out)
{
    json_rec(n, out);
    fputc('\n', out);
}
