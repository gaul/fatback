%{ /* -*-fundamental-*- */
#include <stdlib.h>
#include "vars.h"
#include "interface_data.h"
%}

%union {
     char *string;
}

%token <string> NUMBER 
%token <string> WORD
%%

expression: WORD '=' WORD {set_fbvar($1, $3);}
	|   WORD '=' NUMBER {set_fbvar($1, atol($3));}
	|   NUMBER '=' NUMBER {clusts[atol($1)].fat_entry = atol($3);}
	;

%%
#include <stdio.h>
void yyerror(char *s)
{
	printf("%s\n", s);
}
