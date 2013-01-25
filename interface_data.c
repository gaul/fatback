/***********
 * $Id: interface_data.c,v 1.4 2001/05/30 15:47:03 harbourn Exp $
 * Interface data module for fatback
 * This holds global data that needs to be visible
 * to the interface related portions of fatback.
 * these have been moved to there own file as to
 * not polute the global namespace of fatback.
 ************/

#include <stdio.h>
#include "interface.h"

int stop_code;
clust_t *clusts;
dirent_t *cwd;
vbr_t vbr;
dirent_t *root_dir;
const char delim = '/';  /* this should be changed to a fatback var */

/* a table of commands for the interpreter.  
 * "name", function pointer, "synopsis"
 */
command_t commands[] = {
     {"cd", cmd_cd, "Change to a specified directory"},
     {"cp", cmd_cp, "Copy file(s) to host filesystem"},
     {"copy", cmd_cp, "Synonym for 'cp'"},
     {"help", cmd_help, "Display this text"},
     {"ls", cmd_ls, "List files in a directory"},
     {"dir", cmd_ls, "Synonym for 'ls'"},
     {"pwd", cmd_pwd, "Print the current working directory"},
     {"stat", cmd_stat, "Display detailed information for an entry"},
     {"chain", cmd_chain, "Display cluster chain for an entry"},
     {"cpchain", cmd_cpchain, "Copy a cluster chain to a file"},
     {"lostchains", cmd_lostchains, "Display a list of lost cluster chains"},
     {"sh", cmd_sh, "Execute a command in the outside environment"},
     {"set", cmd_set, "Set runtime variables within fatback"},
     {"done", cmd_done, "Stop undeleting this partition"},
     {"quit", cmd_quit, "Stop undeleting from all partitions"},
     {"exit", cmd_quit, "Synonym for 'quit'"},
     {NULL, NULL, NULL}
};

void show_commands_ptr(void)
{
     printf("show_commands_ptr: %p\n", commands);
}
