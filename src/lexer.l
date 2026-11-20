%option noyywrap yylineno nounput noinput
%{
#include "ast.h"
#include "parser.tab.h"
#include <stdlib.h>
#include <string.h>

static char strbuf[4096];
static int strlen_;

static void str_add(char c)
{
    if (strlen_ < (int)sizeof(strbuf) - 1)
        strbuf[strlen_++] = c;
}
%}

%x COMMENT
%x STRING

DIGIT   [0-9]
IDENT   [A-Za-z_][A-Za-z0-9_]*

%%

"//".*                  ;
"/*"                    BEGIN(COMMENT);
<COMMENT>"*/"           BEGIN(INITIAL);
<COMMENT>.|\n           ;
<COMMENT><<EOF>>        { fprintf(stderr, "line %d: error: unterminated comment\n", yylineno); error_count++; return 0; }

^[ \t]*"#".*            ;

\"                      { strlen_ = 0; BEGIN(STRING); }
<STRING>\"              { strbuf[strlen_] = 0; yylval.str = strdup(strbuf); BEGIN(INITIAL); return STRING_LIT; }
<STRING>\\n             str_add('\n');
<STRING>\\t             str_add('\t');
<STRING>\\\"            str_add('"');
<STRING>\\\\            str_add('\\');
<STRING>\n              { fprintf(stderr, "line %d: error: newline in string literal\n", yylineno); error_count++; BEGIN(INITIAL); }
<STRING>.               str_add(yytext[0]);

"int"                   return INT;
"void"                  return VOID;
"if"                    return IF;
"else"                  return ELSE;
"while"                 return WHILE;
"do"                    return DO;
"for"                   return FOR;
"return"                return RETURN;
"break"                 return BREAK;
"continue"              return CONTINUE;
"print"                 return PRINT;

{DIGIT}+                { yylval.num = atoll(yytext); return NUMBER; }
{IDENT}                 { yylval.str = strdup(yytext); return IDENT; }

"++"                    return INC;
"--"                    return DEC;
"+="                    return ADD_ASSIGN;
"-="                    return SUB_ASSIGN;
"*="                    return MUL_ASSIGN;
"/="                    return DIV_ASSIGN;
"%="                    return MOD_ASSIGN;
"=="                    return EQ;
"!="                    return NE;
"<="                    return LE;
">="                    return GE;
"&&"                    return AND;
"||"                    return OR;
"<<"                    return LSHIFT;
">>"                    return RSHIFT;

[-+*/%=<>!~&|^?:;,(){}] return yytext[0];

[ \t\r\n]+              ;

.                       { fprintf(stderr, "line %d: error: unexpected character '%s'\n", yylineno, yytext); error_count++; }

%%
