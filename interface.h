/* $Id: interface.h,v 1.4 2001/01/25 08:46:45 harbourn Exp $
 * User interface module for fatback
 * (c) 2000 DoD Computer Forensics Lab
 * By Nicholas Harbour
 */
#ifndef INTERFACE_H
#define INTERFACE_H

#include "dirtree.h"
#include "vbr.h"
#include "fat.h"

void interface_init(dirent_t *, clust_t *, vbr_t);
void partition_menu(int, int);
int process_commands(void);
dirent_t *find_in_tree(dirent_t *, dirent_t *, char *);
entlist_t *find_files(int, char **);
void exec_line(char *);
char *fn_car(char *);  /* extract the first element of the filename */
char *fn_cdr(char *);  /* return the remainder after the first element */
char *argvcat(char **);

/* command functions */
void cmd_cd(int, char **);
void cmd_cp(int, char **);
void cmd_help(int, char **);
void cmd_ls(int, char **);
void cmd_pwd(int, char **);
void cmd_stat(int, char **);
void cmd_chain(int, char **);
void cmd_cpchain(int, char **);
void cmd_lostchains(int, char **);
void cmd_sh(int, char **);
void cmd_done(int, char **);
void cmd_quit(int, char **);
void cmd_set(int, char **);

typedef struct {
     char *name;
     void (*func)(int, char **);
     char *help; /* a brief description of the command */
} command_t;

enum {
     STOPCODE_DONE = 1,
     STOPCODE_QUIT = 2
};

#endif /*INTERFACE_H*/

