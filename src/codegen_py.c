#define _GNU_SOURCE
#include "ast.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static FILE *out;
static int indent = 0;

typedef struct Loop {
    Node *node;
    struct Loop *up;
} Loop;

static Loop *loops = NULL;

static char *fmt(const char *f, ...)
{
    char *s;
    va_list ap;
    va_start(ap, f);
    if (vasprintf(&s, f, ap) < 0)
        abort();
    va_end(ap);
    return s;
}

static void line(const char *f, ...)
{
    for (int i = 0; i < indent; i++)
        fputs("    ", out);
    va_list ap;
    va_start(ap, f);
    vfprintf(out, f, ap);
    va_end(ap);
    fputc('\n', out);
}

static int is_rel(const char *op)
{
    return !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "<") || !strcmp(op, ">") ||
           !strcmp(op, "<=") || !strcmp(op, ">=");
}

static char *ex(Node *n);

static char *cond(Node *n)
{
    if (n->kind == N_BIN && is_rel(n->text))
        return fmt("(%s %s %s)", ex(n->kids[0]), n->text, ex(n->kids[1]));
    if (n->kind == N_BIN && !strcmp(n->text, "&&"))
        return fmt("(%s and %s)", cond(n->kids[0]), cond(n->kids[1]));
    if (n->kind == N_BIN && !strcmp(n->text, "||"))
        return fmt("(%s or %s)", cond(n->kids[0]), cond(n->kids[1]));
    if (n->kind == N_UN && n->text[0] == '!')
        return fmt("(not %s)", cond(n->kids[0]));
    return ex(n);
}

static char *py_string(const char *s)
{
    size_t cap = strlen(s) * 2 + 3;
    char *r = malloc(cap), *p = r;
    *p++ = '"';
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { *p++ = '\\'; *p++ = *s; }
        else if (*s == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (*s == '\t') { *p++ = '\\'; *p++ = 't'; }
        else *p++ = *s;
    }
    *p++ = '"';
    *p = 0;
    return r;
}

static char *arith(const char *op, const char *a, const char *b)
{
    if (!strcmp(op, "/"))
        return fmt("_div(%s, %s)", a, b);
    if (!strcmp(op, "%"))
        return fmt("_mod(%s, %s)", a, b);
    return fmt("(%s %s %s)", a, op, b);
}

static char *ex(Node *n)
{
    switch (n->kind) {
    case N_NUM:
        return fmt("%lld", n->val);
    case N_VAR:
        return fmt("%s", n->sym->pyname);
    case N_BIN:
        if (is_rel(n->text) || !strcmp(n->text, "&&") || !strcmp(n->text, "||"))
            return fmt("int(%s)", cond(n));
        return arith(n->text, ex(n->kids[0]), ex(n->kids[1]));
    case N_UN:
        if (n->text[0] == '!')
            return fmt("int(%s)", cond(n));
        return fmt("(%s%s)", n->text, ex(n->kids[0]));
    case N_COND:
        return fmt("(%s if %s else %s)", ex(n->kids[1]), cond(n->kids[0]), ex(n->kids[2]));
    case N_ASSIGN: {
        const char *v = n->kids[0]->sym->pyname;
        char *rhs = ex(n->kids[1]);
        if (n->text[0] == '=')
            return fmt("(%s := %s)", v, rhs);
        char op[2] = { n->text[0], 0 };
        return fmt("(%s := %s)", v, arith(op, (char *)v, rhs));
    }
    case N_INCDEC: {
        const char *v = n->kids[0]->sym->pyname;
        char *upd = fmt("(%s := (%s %c 1))", v, v, n->text[0]);
        if (n->flag)
            return upd;
        return fmt("(%s %c 1)", upd, n->text[0] == '+' ? '-' : '+');
    }
    case N_CALL: {
        char *args = strdup("");
        for (int i = 0; i < n->nkids; i++)
            args = fmt("%s%s%s", args, i ? ", " : "", ex(n->kids[i]));
        return fmt("%s(%s)", n->func->text, args);
    }
    default:
        fprintf(stderr, "codegen: unexpected expression node %s\n", kind_name(n->kind));
        exit(3);
    }
}

static void collect_globals(Node *n, char **names, int *count)
{
    if (!n)
        return;
    if ((n->kind == N_ASSIGN || n->kind == N_INCDEC) && n->kids[0]->sym && n->kids[0]->sym->global) {
        const char *nm = n->kids[0]->sym->pyname;
        int found = 0;
        for (int i = 0; i < *count; i++)
            if (!strcmp(names[i], nm)) found = 1;
        if (!found)
            names[(*count)++] = (char *)nm;
    }
    for (int i = 0; i < n->nkids; i++)
        collect_globals(n->kids[i], names, count);
}

static void stmt(Node *n);

static void body(Node *n)
{
    indent++;
    int before = n->kind == N_BLOCK ? n->nkids : 1;
    if (before == 0)
        line("pass");
    else
        stmt(n);
    indent--;
}

static void simple_expr_stmt(Node *e)
{
    if (e->kind == N_ASSIGN) {
        const char *v = e->kids[0]->sym->pyname;
        char *rhs = ex(e->kids[1]);
        if (e->text[0] == '=')
            line("%s = %s", v, rhs);
        else {
            char op[2] = { e->text[0], 0 };
            line("%s = %s", v, arith(op, (char *)v, rhs));
        }
    } else if (e->kind == N_INCDEC) {
        line("%s %c= 1", e->kids[0]->sym->pyname, e->text[0]);
    } else {
        line("%s", ex(e));
    }
}

static void emit_for_step(Node *f)
{
    Node *step = f->kids[2];
    if (step->kind != N_EMPTY)
        simple_expr_stmt(step);
}

static void stmt(Node *n)
{
    switch (n->kind) {
    case N_BLOCK:
        if (n->nkids == 0)
            line("pass");
        for (int i = 0; i < n->nkids; i++)
            stmt(n->kids[i]);
        break;
    case N_DECLS:
        for (int i = 0; i < n->nkids; i++)
            stmt(n->kids[i]);
        break;
    case N_DECL:
        line("%s = %s", n->sym->pyname, n->nkids ? ex(n->kids[0]) : "0");
        break;
    case N_EXPRSTMT:
        simple_expr_stmt(n->kids[0]);
        break;
    case N_EMPTY:
        break;
    case N_IF:
        line("if %s:", cond(n->kids[0]));
        body(n->kids[1]);
        if (n->nkids == 3) {
            line("else:");
            body(n->kids[2]);
        }
        break;
    case N_WHILE: {
        Loop lp = { n, loops };
        loops = &lp;
        line("while %s:", cond(n->kids[0]));
        body(n->kids[1]);
        loops = lp.up;
        break;
    }
    case N_DOWHILE: {
        Loop lp = { n, loops };
        loops = &lp;
        line("while True:");
        body(n->kids[0]);
        indent++;
        line("if not %s:", cond(n->kids[1]));
        indent++;
        line("break");
        indent -= 2;
        loops = lp.up;
        break;
    }
    case N_FOR: {
        Loop lp = { n, loops };
        loops = &lp;
        stmt(n->kids[0]);
        line("while %s:", n->kids[1]->kind == N_EMPTY ? "True" : cond(n->kids[1]));
        body(n->kids[3]);
        indent++;
        emit_for_step(n);
        indent--;
        loops = lp.up;
        break;
    }
    case N_RETURN:
        if (n->kids[0]->kind == N_EMPTY)
            line("return");
        else
            line("return %s", ex(n->kids[0]));
        break;
    case N_BREAK:
        line("break");
        break;
    case N_CONTINUE:
        if (loops && loops->node->kind == N_FOR)
            emit_for_step(loops->node);
        else if (loops && loops->node->kind == N_DOWHILE) {
            line("if not %s:", cond(loops->node->kids[1]));
            indent++;
            line("break");
            indent--;
        }
        line("continue");
        break;
    case N_PRINT: {
        char *args = strdup("");
        for (int i = 0; i < n->nkids; i++) {
            Node *a = n->kids[i];
            args = fmt("%s%s%s", args, i ? ", " : "", a->kind == N_STR ? py_string(a->text) : ex(a));
        }
        line("print(%s)", args);
        break;
    }
    default:
        fprintf(stderr, "codegen: unexpected statement node %s\n", kind_name(n->kind));
        exit(3);
    }
}

static void function(Node *f)
{
    char *params = strdup("");
    for (int i = 0; i < f->nparams; i++)
        params = fmt("%s%s%s", params, i ? ", " : "", f->local_syms[i]->pyname);
    line("def %s(%s):", f->text, params);
    indent++;
    char *gl[256];
    int gc = 0;
    collect_globals(f->kids[0], gl, &gc);
    if (gc) {
        char *names = strdup("");
        for (int i = 0; i < gc; i++)
            names = fmt("%s%s%s", names, i ? ", " : "", gl[i]);
        line("global %s", names);
    }
    if (f->kids[0]->nkids == 0)
        line("pass");
    else
        stmt(f->kids[0]);
    indent--;
    line("");
}

void gen_python(Node *program, FILE *o)
{
    out = o;
    line("import sys");
    line("");
    line("def _div(a, b):");
    line("    q = abs(a) // abs(b)");
    line("    return q if (a < 0) == (b < 0) else -q");
    line("");
    line("def _mod(a, b):");
    line("    return a - _div(a, b) * b");
    line("");
    for (int i = 0; i < program->nkids; i++)
        if (program->kids[i]->kind == N_DECLS)
            stmt(program->kids[i]);
    line("");
    for (int i = 0; i < program->nkids; i++)
        if (program->kids[i]->kind == N_FUNC)
            function(program->kids[i]);
    line("if __name__ == \"__main__\":");
    line("    sys.exit(main() or 0)");
}
