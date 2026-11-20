%token IF ELSE EXPR OTHER
%%
stmt
    : IF EXPR stmt
    | IF EXPR stmt ELSE stmt
    | OTHER
    ;
