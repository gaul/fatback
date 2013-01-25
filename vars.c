/***********
 * $Id: vars.c,v 1.4 2001/05/30 15:45:19 harbourn Exp $
 * Runtime variable handeling module for fatback
 ***********/

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vars.h"
#include "util.h"

/* This is a symbol table containing all the global variables used by the
 * program.  These can be accessed by the user via the "set" command */
fbvar_t vars[] =
{
     {"verbose", FB_BOOL, {0}},
     {"sectsize", FB_INT, {0}},
     {"prompt", FB_STRING, {0}},
     {"showall", FB_BOOL, {0}},
	 {"deleted_prefix", FB_STRING, {0}},
     {NULL, 0, {0}}
};

/*
 * Change a value of a variable in the vars[] array.
 */
int set_fbvar(char *varname, ...)
{
     int i, found = 0;
     va_list arg_list;

     va_start(arg_list, varname);
     for (i = 0; !found && vars[i].name; i++) {
          if (strcmp(vars[i].name, varname) == 0) {
               unsigned int iarg;
               char *sarg;
               found++;
               switch (vars[i].type) {
               case FB_BOOL: 
                    sarg = va_arg(arg_list, char *);
                    if (strcmp(sarg, "on") == 0)
                         vars[i].val.bval = 1;
                    else if (strcmp(sarg, "off") == 0)
                         vars[i].val.bval = 0;
                    break;
               case FB_INT:
                    iarg = va_arg(arg_list, unsigned int);
                    vars[i].val.ival = iarg;
                    break;
               case FB_STRING:
                    sarg = va_arg(arg_list, char *);
                    vars[i].val.sval = strdup(sarg);
                    break;
               }
          }
     }
     va_end(arg_list);
     return found ? 0 : 1;
}

/*
 * Get the value of a variable in the vars[] array.
 * This currently returns a copy of the structure, this should be changed.
 */
fbvar_t *get_fbvar(char *varname)
{
     int i;
     fbvar_t *retval = NULL;

     for (i = 0; !retval && vars[i].name; i++) 
          if (strcmp(vars[i].name, varname) == 0) {
               retval = emalloc(sizeof *retval);
               memcpy(retval, &vars[i], sizeof *retval);
          }

     return retval;
}

