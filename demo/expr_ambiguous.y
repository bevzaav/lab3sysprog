%token NUM
%%
e
    : e '+' e
    | e '*' e
    | NUM
    ;
