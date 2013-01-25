/***********
 * $Id: cmd_set.c,v 1.3 2001/05/30 15:47:03 harbourn Exp $
 * set command for fatback
 * this command is particularly important 
 * because it can be used to control many
 * aspects of the programs operation in 
 * a runtime environment. 
 ***********/

#include <stdio.h>
#include "interface.h"
#include "vars.h"
#include "output.h"

char *set_arg = NULL;
char *set_arg_pos = NULL;

void cmd_set(int argc, char *argv[])
{
     extern void yyparse();
     extern void yy_scan_string();

     /* if the user just types 'set', then display the
      * the current state of all the variables */
     if (argc == 1) {
          extern fbvar_t vars[];
          int i;
      
          for (i = 0; vars[i].name; i++) {
               char *name;
               name = vars[i].name;
               display(NORMAL, "%s=", name);
               switch (vars[i].type) {
               case FB_INT: 
                    display(NORMAL, "%u\n", vars[i].val.ival);
                    break;
               case FB_STRING:
                    display(NORMAL, "%s\n", vars[i].val.sval ? vars[i].val.sval : "");
                    break;
               case FB_BOOL:
                    display(NORMAL, "%s\n", vars[i].val.bval ? "on" : "off");
                    break;
               }
          }
          return;
     }
     /* if the user inputs an expression, then let
      * the lex and yacc parser handel it */
     set_arg = argvcat(&argv[1]);
     set_arg_pos = set_arg;
     yy_scan_string(set_arg);
     yyparse();
}
