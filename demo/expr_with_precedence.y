%token NUM
%left '+'
%left '*'
%%
e
    : e '+' e
    | e '*' e
    | NUM
    ;
