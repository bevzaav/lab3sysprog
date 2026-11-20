#include "ast.h"
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;
extern int yylex(void);
extern int yyparse(void);
extern int yylineno;
extern Node *program_root;
extern YYSTYPE yylval;
const char *token_name(int tok);

static void usage(const char *prog)
{
    fprintf(stderr,
            "usage: %s file.c [options]\n"
            "  --tokens          print the token stream (lexer output)\n"
            "  --ast             print the AST as an indented tree\n"
            "  --json FILE       write the AST as JSON (for the web viewer)\n"
            "  --py FILE         translate to Python and write FILE\n"
            "  --run             interpret the program (default if nothing else is requested)\n",
            prog);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    const char *path = argv[1];
    int want_tokens = 0, want_ast = 0, want_run = 0;
    const char *json_path = NULL, *py_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--tokens")) want_tokens = 1;
        else if (!strcmp(argv[i], "--ast")) want_ast = 1;
        else if (!strcmp(argv[i], "--run")) want_run = 1;
        else if (!strcmp(argv[i], "--json") && i + 1 < argc) json_path = argv[++i];
        else if (!strcmp(argv[i], "--py") && i + 1 < argc) py_path = argv[++i];
        else { usage(argv[0]); return 1; }
    }
    if (!want_tokens && !want_ast && !want_run && !json_path && !py_path)
        want_run = 1;

    yyin = fopen(path, "r");
    if (!yyin) {
        perror(path);
        return 1;
    }

    if (want_tokens) {
        int tok;
        while ((tok = yylex()) != 0) {
            printf("line %-3d %-12s", yylineno, token_name(tok));
            if (tok == NUMBER) printf(" %lld", yylval.num);
            else if (tok == IDENT || tok == STRING_LIT) printf(" %s", yylval.str);
            printf("\n");
        }
        return error_count ? 1 : 0;
    }

    if (yyparse() != 0) {
        fprintf(stderr, "%d error(s), aborting\n", error_count ? error_count : 1);
        return 1;
    }
    resolve_calls(program_root);
    if (error_count) {
        fprintf(stderr, "%d error(s), aborting\n", error_count);
        return 1;
    }

    if (want_ast)
        dump_tree(program_root, stdout);
    if (json_path) {
        FILE *f = fopen(json_path, "w");
        if (!f) { perror(json_path); return 1; }
        dump_json(program_root, f);
        fclose(f);
    }
    if (py_path) {
        FILE *f = fopen(py_path, "w");
        if (!f) { perror(py_path); return 1; }
        gen_python(program_root, f);
        fclose(f);
    }
    if (want_run) {
        fflush(stdout);
        int rc = interpret(program_root);
        fflush(stdout);
        fprintf(stderr, "[exit code %d]\n", rc);
        return rc;
    }
    return 0;
}
