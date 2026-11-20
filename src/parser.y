%{
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex(void);
extern int yylineno;
extern char *yytext;
void yyerror(const char *msg);

Node *program_root = NULL;

static Node *mk_binary(const char *op, Node *l, Node *r)
{
    return mk(N_BIN, op, l, r, NULL);
}

static Node *mk_var_ref(char *name)
{
    Node *n = mk(N_VAR, name, NULL);
    Sym *s = lookup_var(name);
    if (!s)
        sem_error(yylineno, "undeclared variable '%s'", name);
    n->sym = s;
    return n;
}

static Node *mk_assign(char *name, const char *op, Node *rhs)
{
    Node *target = mk_var_ref(name);
    return mk(N_ASSIGN, op, target, rhs, NULL);
}

static Node *mk_incdec(char *name, const char *op, int prefix)
{
    Node *n = mk(N_INCDEC, op, mk_var_ref(name), NULL);
    n->flag = prefix;
    return n;
}

static Node *mk_decl(char *name, Node *init)
{
    Node *n = mk(N_DECL, name, NULL);
    if (init)
        add_kid(n, init);
    n->sym = declare_var(name, yylineno);
    return n;
}
%}

%union {
    long long num;
    char *str;
    struct Node *node;
}

%error-verbose
%expect 0

%token <num> NUMBER
%token <str> IDENT STRING_LIT
%token INT VOID IF ELSE WHILE DO FOR RETURN BREAK CONTINUE PRINT
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token EQ NE LE GE AND OR LSHIFT RSHIFT

%type <node> program external function global_decl block_open_body stmt stmt_list decl declarator_list declarator
%type <node> expr opt_expr for_init arg_list arg_list_opt param_list param_list_opt print_arg print_args
%type <node> compound

%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%right '?' ':'
%left OR
%left AND
%left '|'
%left '^'
%left '&'
%left EQ NE
%left '<' '>' LE GE
%left LSHIFT RSHIFT
%left '+' '-'
%left '*' '/' '%'
%right UMINUS '!' '~' PREFIX
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%%

program
    : external                  { program_root = mk(N_PROGRAM, "program", $1, NULL); $$ = program_root; }
    | program external          { add_kid($1, $2); $$ = $1; }
    ;

external
    : function                  { $$ = $1; }
    | global_decl               { $$ = $1; }
    ;

global_decl
    : INT declarator_list ';'   { $$ = $2; }
    ;

function
    : INT IDENT '(' {
            Node *f = mk(N_FUNC, $2, NULL);
            f->is_void = 0;
            func_begin(f);
            $<node>$ = f;
        } param_list_opt ')' {
            Node *f = $<node>4;
            f->nparams = f->nlocals;
            register_func(f);
        } '{' block_open_body '}' {
            Node *f = $<node>4;
            add_kid(f, $9);
            func_end();
            $$ = f;
        }
    | VOID IDENT '(' {
            Node *f = mk(N_FUNC, $2, NULL);
            f->is_void = 1;
            func_begin(f);
            $<node>$ = f;
        } param_list_opt ')' {
            Node *f = $<node>4;
            f->nparams = f->nlocals;
            register_func(f);
        } '{' block_open_body '}' {
            Node *f = $<node>4;
            add_kid(f, $9);
            func_end();
            $$ = f;
        }
    ;

param_list_opt
    : /* empty */               { $$ = NULL; }
    | VOID                      { $$ = NULL; }
    | param_list                { $$ = $1; }
    ;

param_list
    : INT IDENT                 { declare_var($2, yylineno); $$ = NULL; }
    | param_list ',' INT IDENT  { declare_var($4, yylineno); $$ = NULL; }
    ;

block_open_body
    : stmt_list                 { $$ = $1; }
    ;

compound
    : '{' { scope_push(); } stmt_list '}'   { scope_pop(); $$ = $3; }
    ;

stmt_list
    : /* empty */               { $$ = mk(N_BLOCK, "block", NULL); }
    | stmt_list stmt            { add_kid($1, $2); $$ = $1; }
    ;

stmt
    : compound                  { $$ = $1; }
    | expr ';'                  { $$ = mk(N_EXPRSTMT, "expr", $1, NULL); }
    | ';'                       { $$ = mk(N_EMPTY, "empty", NULL); }
    | decl ';'                  { $$ = $1; }
    | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                { $$ = mk(N_IF, "if", $3, $5, NULL); }
    | IF '(' expr ')' stmt ELSE stmt
                                { $$ = mk(N_IF, "if", $3, $5, $7, NULL); }
    | WHILE '(' expr ')' stmt   { $$ = mk(N_WHILE, "while", $3, $5, NULL); }
    | DO stmt WHILE '(' expr ')' ';'
                                { $$ = mk(N_DOWHILE, "do-while", $2, $5, NULL); }
    | FOR '(' { scope_push(); } for_init ';' opt_expr ';' opt_expr ')' stmt
                                { scope_pop(); $$ = mk(N_FOR, "for", $4, $6, $8, $10, NULL); }
    | RETURN opt_expr ';'       { $$ = mk(N_RETURN, "return", $2, NULL); }
    | BREAK ';'                 { $$ = mk(N_BREAK, "break", NULL); }
    | CONTINUE ';'              { $$ = mk(N_CONTINUE, "continue", NULL); }
    | PRINT '(' print_args ')' ';'
                                { $$ = $3; }
    ;

print_args
    : print_arg                 { $$ = mk(N_PRINT, "print", $1, NULL); }
    | print_args ',' print_arg  { add_kid($1, $3); $$ = $1; }
    ;

print_arg
    : expr                      { $$ = $1; }
    | STRING_LIT                { $$ = mk(N_STR, $1, NULL); }
    ;

decl
    : INT declarator_list       { $$ = $2; }
    ;

declarator_list
    : declarator                { $$ = mk(N_DECLS, "declarations", $1, NULL); }
    | declarator_list ',' declarator
                                { add_kid($1, $3); $$ = $1; }
    ;

declarator
    : IDENT                     { $$ = mk_decl($1, NULL); }
    | IDENT '=' expr            { $$ = mk_decl($1, $3); }
    ;

for_init
    : /* empty */               { $$ = mk(N_EMPTY, "empty", NULL); }
    | decl                      { $$ = $1; }
    | expr                      { $$ = mk(N_EXPRSTMT, "expr", $1, NULL); }
    ;

opt_expr
    : /* empty */               { $$ = mk(N_EMPTY, "empty", NULL); }
    | expr                      { $$ = $1; }
    ;

expr
    : NUMBER                    { $$ = mk_num($1); }
    | IDENT                     { $$ = mk_var_ref($1); }
    | '(' expr ')'              { $$ = $2; }
    | IDENT '(' arg_list_opt ')'
                                { $$ = $3; $$->text = $1; }
    | IDENT '=' expr            { $$ = mk_assign($1, "=", $3); }
    | IDENT ADD_ASSIGN expr     { $$ = mk_assign($1, "+=", $3); }
    | IDENT SUB_ASSIGN expr     { $$ = mk_assign($1, "-=", $3); }
    | IDENT MUL_ASSIGN expr     { $$ = mk_assign($1, "*=", $3); }
    | IDENT DIV_ASSIGN expr     { $$ = mk_assign($1, "/=", $3); }
    | IDENT MOD_ASSIGN expr     { $$ = mk_assign($1, "%=", $3); }
    | INC IDENT %prec PREFIX    { $$ = mk_incdec($2, "++", 1); }
    | DEC IDENT %prec PREFIX    { $$ = mk_incdec($2, "--", 1); }
    | IDENT INC                 { $$ = mk_incdec($1, "++", 0); }
    | IDENT DEC                 { $$ = mk_incdec($1, "--", 0); }
    | expr '?' expr ':' expr    { $$ = mk(N_COND, "?:", $1, $3, $5, NULL); }
    | expr OR expr              { $$ = mk_binary("||", $1, $3); }
    | expr AND expr             { $$ = mk_binary("&&", $1, $3); }
    | expr '|' expr             { $$ = mk_binary("|", $1, $3); }
    | expr '^' expr             { $$ = mk_binary("^", $1, $3); }
    | expr '&' expr             { $$ = mk_binary("&", $1, $3); }
    | expr EQ expr              { $$ = mk_binary("==", $1, $3); }
    | expr NE expr              { $$ = mk_binary("!=", $1, $3); }
    | expr '<' expr             { $$ = mk_binary("<", $1, $3); }
    | expr '>' expr             { $$ = mk_binary(">", $1, $3); }
    | expr LE expr              { $$ = mk_binary("<=", $1, $3); }
    | expr GE expr              { $$ = mk_binary(">=", $1, $3); }
    | expr LSHIFT expr          { $$ = mk_binary("<<", $1, $3); }
    | expr RSHIFT expr          { $$ = mk_binary(">>", $1, $3); }
    | expr '+' expr             { $$ = mk_binary("+", $1, $3); }
    | expr '-' expr             { $$ = mk_binary("-", $1, $3); }
    | expr '*' expr             { $$ = mk_binary("*", $1, $3); }
    | expr '/' expr             { $$ = mk_binary("/", $1, $3); }
    | expr '%' expr             { $$ = mk_binary("%", $1, $3); }
    | '-' expr %prec UMINUS     { $$ = mk(N_UN, "-", $2, NULL); }
    | '!' expr                  { $$ = mk(N_UN, "!", $2, NULL); }
    | '~' expr                  { $$ = mk(N_UN, "~", $2, NULL); }
    ;

arg_list_opt
    : /* empty */               { $$ = mk(N_CALL, "", NULL); }
    | arg_list                  { $$ = $1; }
    ;

arg_list
    : expr                      { $$ = mk(N_CALL, "", $1, NULL); }
    | arg_list ',' expr         { add_kid($1, $3); $$ = $1; }
    ;

%%

void yyerror(const char *msg)
{
    fprintf(stderr, "line %d: %s\n", yylineno, msg);
    error_count++;
}

const char *token_name(int tok)
{
    if (tok < 256) {
        static char buf[4];
        buf[0] = (char)tok;
        buf[1] = 0;
        return buf;
    }
    return yytname[YYTRANSLATE(tok)];
}
