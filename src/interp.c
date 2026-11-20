#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GLOBALS 1024
#define MAX_DEPTH 5000

enum { FL_NONE, FL_BREAK, FL_CONT, FL_RET };

static long long globals[MAX_GLOBALS];
static int depth = 0;

static void runtime_error(Node *n, const char *msg)
{
    fprintf(stderr, "runtime error (line %d): %s\n", n->line, msg);
    exit(2);
}

static long long eval(Node *n, long long *loc);
static int exec(Node *n, long long *loc, long long *ret);

static long long *slot(Node *var, long long *loc)
{
    return var->sym->global ? &globals[var->sym->idx] : &loc[var->sym->idx];
}

static long long apply_binary(Node *n, const char *op, long long a, long long b)
{
    if (!strcmp(op, "+")) return a + b;
    if (!strcmp(op, "-")) return a - b;
    if (!strcmp(op, "*")) return a * b;
    if (!strcmp(op, "/")) {
        if (b == 0) runtime_error(n, "division by zero");
        return a / b;
    }
    if (!strcmp(op, "%")) {
        if (b == 0) runtime_error(n, "modulo by zero");
        return a % b;
    }
    if (!strcmp(op, "<<")) return a << b;
    if (!strcmp(op, ">>")) return a >> b;
    if (!strcmp(op, "&")) return a & b;
    if (!strcmp(op, "|")) return a | b;
    if (!strcmp(op, "^")) return a ^ b;
    if (!strcmp(op, "==")) return a == b;
    if (!strcmp(op, "!=")) return a != b;
    if (!strcmp(op, "<")) return a < b;
    if (!strcmp(op, ">")) return a > b;
    if (!strcmp(op, "<=")) return a <= b;
    if (!strcmp(op, ">=")) return a >= b;
    fprintf(stderr, "internal error: unknown binary operator '%s'\n", op);
    exit(3);
}

static long long call_function(Node *call, long long *loc)
{
    Node *fn = call->func;
    if (++depth > MAX_DEPTH)
        runtime_error(call, "call stack overflow");
    long long *frame = calloc(fn->nlocals > 0 ? fn->nlocals : 1, sizeof(long long));
    for (int i = 0; i < call->nkids; i++)
        frame[i] = eval(call->kids[i], loc);
    long long ret = 0;
    exec(fn->kids[0], frame, &ret);
    free(frame);
    depth--;
    return ret;
}

static long long eval(Node *n, long long *loc)
{
    switch (n->kind) {
    case N_NUM:
        return n->val;
    case N_VAR:
        return *slot(n, loc);
    case N_BIN: {
        const char *op = n->text;
        if (!strcmp(op, "&&"))
            return eval(n->kids[0], loc) && eval(n->kids[1], loc);
        if (!strcmp(op, "||"))
            return eval(n->kids[0], loc) || eval(n->kids[1], loc);
        long long a = eval(n->kids[0], loc);
        long long b = eval(n->kids[1], loc);
        return apply_binary(n, op, a, b);
    }
    case N_UN: {
        long long a = eval(n->kids[0], loc);
        if (n->text[0] == '-') return -a;
        if (n->text[0] == '!') return !a;
        return ~a;
    }
    case N_COND:
        return eval(n->kids[0], loc) ? eval(n->kids[1], loc) : eval(n->kids[2], loc);
    case N_ASSIGN: {
        long long rhs = eval(n->kids[1], loc);
        long long *p = slot(n->kids[0], loc);
        if (n->text[0] != '=') {
            char op[2] = { n->text[0], 0 };
            rhs = apply_binary(n, op, *p, rhs);
        }
        *p = rhs;
        return rhs;
    }
    case N_INCDEC: {
        long long *p = slot(n->kids[0], loc);
        long long old = *p;
        *p = old + (n->text[0] == '+' ? 1 : -1);
        return n->flag ? *p : old;
    }
    case N_CALL:
        return call_function(n, loc);
    default:
        fprintf(stderr, "internal error: cannot evaluate node %s\n", kind_name(n->kind));
        exit(3);
    }
}

static int exec(Node *n, long long *loc, long long *ret)
{
    switch (n->kind) {
    case N_BLOCK:
        for (int i = 0; i < n->nkids; i++) {
            int f = exec(n->kids[i], loc, ret);
            if (f != FL_NONE)
                return f;
        }
        return FL_NONE;
    case N_DECLS:
        for (int i = 0; i < n->nkids; i++)
            exec(n->kids[i], loc, ret);
        return FL_NONE;
    case N_DECL: {
        long long v = n->nkids ? eval(n->kids[0], loc) : 0;
        *slot(n, loc) = v;
        return FL_NONE;
    }
    case N_EXPRSTMT:
        eval(n->kids[0], loc);
        return FL_NONE;
    case N_EMPTY:
        return FL_NONE;
    case N_IF:
        if (eval(n->kids[0], loc))
            return exec(n->kids[1], loc, ret);
        if (n->nkids == 3)
            return exec(n->kids[2], loc, ret);
        return FL_NONE;
    case N_WHILE:
        while (eval(n->kids[0], loc)) {
            int f = exec(n->kids[1], loc, ret);
            if (f == FL_BREAK) break;
            if (f == FL_RET) return f;
        }
        return FL_NONE;
    case N_DOWHILE:
        do {
            int f = exec(n->kids[0], loc, ret);
            if (f == FL_BREAK) break;
            if (f == FL_RET) return f;
        } while (eval(n->kids[1], loc));
        return FL_NONE;
    case N_FOR: {
        exec(n->kids[0], loc, ret);
        while (n->kids[1]->kind == N_EMPTY || eval(n->kids[1], loc)) {
            int f = exec(n->kids[3], loc, ret);
            if (f == FL_BREAK) break;
            if (f == FL_RET) return f;
            if (n->kids[2]->kind != N_EMPTY)
                eval(n->kids[2], loc);
        }
        return FL_NONE;
    }
    case N_RETURN:
        *ret = n->kids[0]->kind == N_EMPTY ? 0 : eval(n->kids[0], loc);
        return FL_RET;
    case N_BREAK:
        return FL_BREAK;
    case N_CONTINUE:
        return FL_CONT;
    case N_PRINT:
        for (int i = 0; i < n->nkids; i++) {
            if (i)
                putchar(' ');
            if (n->kids[i]->kind == N_STR)
                fputs(n->kids[i]->text, stdout);
            else
                printf("%lld", eval(n->kids[i], loc));
        }
        putchar('\n');
        return FL_NONE;
    default:
        if (n->kind == N_ASSIGN || n->kind == N_CALL || n->kind == N_INCDEC) {
            eval(n, loc);
            return FL_NONE;
        }
        fprintf(stderr, "internal error: cannot execute node %s\n", kind_name(n->kind));
        exit(3);
    }
}

int interpret(Node *program)
{
    for (int i = 0; i < program->nkids; i++) {
        Node *top = program->kids[i];
        if (top->kind == N_DECLS)
            exec(top, NULL, NULL);
    }
    Node call = {0};
    call.kind = N_CALL;
    call.text = "main";
    call.func = find_func("main");
    call.nkids = 0;
    if (call.func->nparams != 0) {
        fprintf(stderr, "error: main must take no parameters\n");
        return 1;
    }
    return (int)call_function(&call, NULL);
}
