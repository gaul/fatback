/**************
 * $Id: cmd_misc.c,v 1.4 2001/05/30 15:47:02 harbourn Exp $
 * This module contains commands that are really to small to
 * deserve there own file, mainly "one-liners."
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "interface_data.h"
#include "interface.h"
#include "output.h"

/*
 * The change directory command
 */
void cmd_cd(int argc, char *argv[])
{
     dirent_t *dir;

     assert(argv);
     if (!argv[1]) {
          display(NORMAL, "Usage: cd directory\n");
          return;
     }
     if ((dir = find_in_tree(cwd, cwd->child, argv[1])) && (dir->attrs & ATTR_DIR))
          cwd = dir;
     else
          display(NORMAL, "CD: Invalid directory\n");
}

/*
 * The command to display the help menu,
 * which at this point is just a list of
 * the commands and a brief description
 */
void cmd_help(int argc, char *argv[])
{
     command_t *cmd;
     int printed, longest = 0;
     
     /* first calculate the longest command name */
     for (cmd = commands; cmd->name != NULL; cmd++)
          if (strlen(cmd->name) > longest)
               longest = strlen(cmd->name);

     /* now loop through the commands, and the
      * name and description of each */
     for (cmd = commands; cmd->name != NULL; cmd++) {
          int i;
          printed = display(NORMAL, "%s", cmd->name);
          for (i = 0; i < longest + 2 - printed; i++) 
               display(NORMAL, " ");
          display(NORMAL, "%s\n", cmd->help);
     }
}

/*
 * The pwd command Prints the Working Directory
 */
void cmd_pwd(int argc, char *argv[])
{
     display(NORMAL, "%s\n", cwd->lfn ? cwd->lfn : cwd->filename);
}

/*
 * The done command signals that the user
 * is finished processing this partition
 */
void cmd_done(int argc, char *argv[])
{
     stop_code = STOPCODE_DONE;
}

/*
 * The quit command signals that the user
 * is finished running fatback 
 */
void cmd_quit(int argc, char *argv[])
{
     stop_code = STOPCODE_QUIT;
}
