%{ /* -*-fundamental-*- */
/* $Id: setl.l,v 1.4 2001/04/30 09:24:37 harbourn Exp $ 
 * set command for fatback
 * lex script portion
 */
#include <string.h>
#include "sety.h"
int fatback_yyinput(char *, int);
#undef YY_INPUT
#define YY_INPUT(b, r, ms) (r = fatback_yyinput(b, ms))
%}

%%

[0-9]+		{yylval.string = strdup(yytext); return NUMBER;}
[a-zA-Z_>#]+	{yylval.string = strdup(yytext); return WORD;}
[ \t]		{;}
  /*\n		{return 0;}*/
.		{return yytext[0];}

%%

#include <stdlib.h>
#include <string.h>
extern char *set_arg;
extern char *set_arg_pos;

int fatback_yyinput(char *buf, int max_size)
{
	int n;
	if (!set_arg_pos)
		set_arg_pos = set_arg;
	n = (max_size > strlen(set_arg_pos)) ? max_size : strlen(set_arg_pos);
	printf("%p\n", set_arg_pos);
	if (n > 0) {
		memcpy(buf, set_arg_pos, n);
		set_arg_pos += n;
	}
	return n;
}

