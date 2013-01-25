/* $Id: interface.c,v 1.9 2001/05/30 15:47:03 harbourn Exp $
 * This module handles user interface processing (via commands)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <unistd.h>
#include <errno.h>
#include "dirtree.h"
#include "util.h"
#include "fat.h"
#include "vbr.h"
#include "output.h"
#include "recovery.h"
#include "fatback.h"
#include "interface.h"
#include "interface_data.h"
#include "vars.h"

/* to change to libreadline, uncomment the
 * #include and comment out the prototype */

/*#include <readline/readline.h>*/
static char *readline(char *);
static int ending(char *);

typedef char* (*cmdline_hook_t)(char *);

static char *stripwhite(char *);
static char *stripcomments(char *);
static char *strippipe(char *);
static char *pipe_scan(char *);
static int whitespace(char);

static command_t *find_command(char *);
static char **split_line(char *);
static char *cmdline_car(char *, int *);

/*
 * Display a menu of the possible partitions, 
 * and prompt the user as to which one to recover.
 * let them undelete that partition, and keep looping
 * until we recieve the stop code.
 */
void partition_menu(int num_parts, int flags)
{
     int i; 
     unsigned part;
     char *choice;

     /* if only one partition exists, dont bother with the menu */
     if (num_parts == 1) {
          display(VERBOSE, "Only one partition detected: Entering single partition mode\n");
          undel_partition(0, flags);
          return;
     }

     for (;;) {
          display(NORMAL, "Please select one of the following partitions:");
          for (i = 0; i < num_parts; i++)
               display(NORMAL, "  %d", i);
          display(NORMAL, "\n");
          choice = readline(">");
          display(LOGONLY, "%s\n");
          part = atoi(choice);
          free(choice);
          if (part > num_parts)
               display(NORMAL, "Invalid partition number\n");
          else if (undel_partition(part, flags) == STOPCODE_QUIT)
               return;
     }
}

/*
 * Initialize variables in the interface.  
 * this should be called each time a new partition is to be edited
 */
void interface_init(dirent_t *tree, clust_t *clust_array, vbr_t myvbr)
{
     assert(tree && clust_array && myvbr);
     stop_code = 0;
     cwd = root_dir = tree;
     clusts = clust_array;
     vbr = myvbr;
}     

/*
 * Prompt the user for a command and process that command.
 */
int process_commands(void)
{
     char *line, *s;
     
     while (!stop_code) {
          fbvar_t *prompt_var;
          char *tmp_prompt, *prompt;
          char *pipe_command;
          FILE *tmp_pipe;
          errno = 0;
          prompt_var = get_fbvar("prompt");
          tmp_prompt = prompt_var->val.sval;
          free(prompt_var);
          prompt = emalloc(strlen(tmp_prompt) + 2);
          strcpy(prompt, tmp_prompt);
          prompt[strlen(tmp_prompt)] = ' ';
          prompt[strlen(tmp_prompt) + 1] = '\0';

          display(LOGONLY, "%s", prompt);
          line = readline(prompt ? prompt : "> ");
          free(prompt);
          if (!line)
               break;
          if ((pipe_command = pipe_scan(line)) != NULL) {
               if ((tmp_pipe = popen(pipe_command, "w")) == NULL) {
                    perror("cannot create pipe");
                    break;
               }
               set_ostream(tmp_pipe);
          }
          exec_line(line);
          free(line);
          if (pipe_command) {
               free(pipe_command);
               pclose(tmp_pipe);
               reset_ostream();
          }
     }
     return stop_code;
}

/*
 * Execute a command as specified by argument.
 */
void exec_line(char *line)
{
     command_t *command;
     char *newline;
     char **argv;
     int argc, i;
     cmdline_hook_t cmdline_hooks[] = 
     {
          stripcomments,
          strippipe,
          stripwhite,
          NULL
     };

     assert(line);

     /* skip the line if it is a comment */
     if (line[0] == '#')
          return;
     
     newline = strdup(line);
     /* log this line to the audit log. */
     display(LOGONLY, "%s\n", line);

     /* Apply the command line hooks */
     for (i = 0; cmdline_hooks[i]; i++) {
          char *tmp = (*cmdline_hooks[i])(newline);
          free(newline);
          newline = tmp;
     }

     /* split the line into an argv[] array */
     if (!(argv = split_line(newline)))
          return;
     /* make argc the number of argv elements */
     for (argc = 0; argv[argc]; argc++);

     /* look up the command in the command table */
     command = find_command(argv[0]);
     if (!command) {
          display(NORMAL, "Invalid command\n");
          return;
     }
     (*(command->func))(argc, argv); /* run the command! */
     
     /* free newly split command line */
     for (i = 0; i < argc; i++)
          free(argv[i]);
     free(argv);
}

/*
 * Concatenate an array of strings.
 * Works the opposite of splitline.
 */
char *argvcat(char *argv[])
{
     char *retval = NULL;
     int i;

     /* known bug:  this algorithm leaves a trailing space at the
      * end of each line it creates. */

     for (i = 0; argv[i]; i++) {
          char *tmp = retval;
          int retval_len;
          retval_len = (retval ? strlen(retval) : 0) + strlen(argv[i]) + 2;
          retval = emalloc(retval_len);
          *retval = '\0';
          if (tmp)
               strcpy(retval, tmp);
          strcat(retval, argv[i]);
          /* terminate each string with a space and a null */
          retval[retval_len - 2] = ' ';
          retval[retval_len - 1] = '\0'; 
          if (tmp) 
               free(tmp);
     }
     return retval;
}

/*
 * Lookup a command in the command table
 */
static command_t *find_command(char *name)
{
     int i;

     /* the names of commands are simply strings, so just 
      * loop over the commands[] array strcmp'ing the names
      */

     if (!name)
          return NULL;
     for (i = 0; commands[i].name; i++) {
          if (strcmp(name, commands[i].name) == 0)
               return &commands[i];
     }
     return NULL;
}

/*
 * Strip the white space from the ends of a string
 */
static char *stripwhite(char *string)
{
     char *retval;
     int i, head_ws, tail_ws;
     int retvallen, stringlen;

     assert(string);
     stringlen = strlen(string);

     if (stringlen == 0)
          return strdup(string);

     /* count the initial whitespace */
     for (i = 0; whitespace(string[i]); i++)
          ;
     head_ws = i;
     
     /* count the amount of tail whitespace */
     for (i = stringlen - 1; (i >= 0) && whitespace(string[i]); i--)
          ;
     tail_ws = stringlen - (i + 1);
     retvallen = stringlen - (head_ws + tail_ws);

     /* create the new string */
     retval = emalloc(retvallen + 1);
     strncpy(retval, &string[head_ws], retvallen);
     retval[retvallen] = '\0';

     return retval;
}
     
/*
 * Strip out every thing on a line after 
 * the comment character ('#')
 */
static char *stripcomments(char *string)
{
     int i;
     char *retval;

     assert(string);
     /* Find the comment (if any) on the line */
     for (i = 0; string[i] && (string[i] != '#'); i++)
          ;
     retval = emalloc(i + 1);
     strncpy(retval, string, i);
     retval[i] = '\0';
     
     return retval;
}

/*
 * Look up a file in a directory tree based on a name
 */
dirent_t *find_in_tree(dirent_t *dir, dirent_t *entry, char *name)
{
     char *entry_name, *remainder;
     dirent_t *ent;

     /* this funciton works by recursively breaking apart a file name
      * into its layers of directories.
      */

     assert(dir && name);

     /* find the first part of the file name */
     entry_name = fn_car(name);
     remainder = fn_cdr(name);
     if (!entry_name) {
          if (name[0] == delim)
               return root_dir;
          else
               return NULL;
     }

     if (strcmp(entry_name, ".") == 0) {
          free(entry_name);
          if (!remainder)
               return dir;
          else
               return find_in_tree(dir, dir->child, remainder);
     }

     if (strcmp(entry_name, "..") == 0) {
          free(entry_name);
          if (!remainder)
               return dir->parent;
          else
               return find_in_tree(dir->parent, dir->parent->child, remainder);
     }
      
     if (!entry)
          return NULL;

     /* find an entry in the current directory that matches entry_name */
     for (ent = entry; ent; ent = ent->next) {
          dirent_t *matched_ent = NULL;
          int match, match_lfn;

          match = fnmatch(entry_name, ent->filename, 0);
          if (ent->lfn)
               match_lfn = fnmatch(entry_name, ent->lfn, 0);
          else
               match_lfn = FNM_NOMATCH;
          if (match == 0 || match_lfn == 0) {
               if (!remainder) {
                    free(entry_name);
                    return ent;
               } else if (ent->attrs & ATTR_DIR) {
                    matched_ent = find_in_tree(ent, ent->child, remainder);
                    if (matched_ent) {
                         free(entry_name);
                         free(remainder);
                         return matched_ent;
                    }
               }
          }
     }
     free(entry_name);
     if (remainder)
          free(remainder);
     return NULL;
}

/*
 * givin an array of strings (probably from an argv[]) 
 * find all the files that match, put them into a linked
 * list and return them.
 */
entlist_t *find_files(int num, char *strings[])
{
     unsigned i;
     entlist_t *list_head = NULL, *list_tail = NULL;

     /* loop over all arguments */
     for (i = 0; i < num; i++) {
          /* find all entries matching that pattern */
          dirent_t *ent, *next = cwd->child;
          int found = 0;
          while (next && (ent = find_in_tree(cwd, next, strings[i]))) {
               entlist_t *tmp = emalloc(sizeof *tmp);
               found++;
               tmp->next = NULL;
               tmp->ent = ent;
               if (!list_head)
                    list_head = tmp;
               else
                    list_tail->next = tmp;
               list_tail = tmp;
               next = ent->next;
          }
     }
     return list_head;
}

/*
 * Extract the first piece of a file name.
 * ("car" comes from the lisp primitive car, which
 * means take the first element of a list.)
 */
char *fn_car(char *name)
{
     int i=0, start, length;
     char *retval;
     
     assert(name);
     /* first we find the length of the first part */
     if (name[0] == delim)
          i++;
     start = i;
     while (name[i] != '\0' && name[i] != delim)
          i++;
     length = i - start;
     
     /* allocate space for our new string */
     if (length == 0)
          return NULL;
     retval = emalloc(length + 1);
     
     /* copy the fragment into the new string */
     strncpy(retval, &name[start], length);
     retval[length] = '\0';
     return retval;
}

/* Extract all but the first piece of a file name.
 * (similar to "car", "cdr" is taken from lisp as
 * well, it means take all but the first element of
 * a list.)
 */
char *fn_cdr(char *name)
{
     int i=0, start, length;
     char *retval;

     assert(name);
     /* find the length of the remainder */
     if (name[0] == delim)
          i++;
     while (name[i] != '\0' && name[i] != delim)
          i++;
     start = i + 1; /* increment past the delimeter */
     if (name[i] == '\0')
          return NULL;
     while (name[i] != '\0')
          i++;
     length = i - start;
     
     /* allocate space for our new string */
     if (length == 0)
          return NULL;
     retval = emalloc(length + 1);
     
     /* copy the remainder into the new string */
     strncpy(retval, &name[start], length);
     retval[length] = '\0';
     return retval;
}

/*
 * Take all but the last portion of a file name.
 * (There is no lisp primitive for rcdr, I made it up.
 */
char *fn_rcdr(char *name)
{
     int i, name_strlen;
     char *retval;

     assert(name);

     /* calculate the length of the name string */
     name_strlen = strlen(name);
     if (!name_strlen)
          return NULL;
     /* position the index at the end of the string */
     i = name_strlen - 1; 

     /* back up to before the delimeter, if any */
     if (name[name_strlen - 1] == delim)
          i--;
     if (i < 0)
          return NULL;
     /* step backwards through the string until we
      * find a delimeter, or we hit the beginning */
     while ((i >= 0) && (name[i] != delim))
          i--;
     /* place all the data up to the index into a new string */
     if (i < 0)
          return NULL;
     retval = emalloc(i + 2);
     strncpy(retval, name, i + 1);
     return retval;
}

/*
 * Take a string delimeted by whitespace, and form
 * it into an array of strings.
 */
static char **split_line(char *line)
{
     int i, total = 0;
     char **list = NULL;

     for (i = 0; line[i] != '\0'; ) {
          char *word = cmdline_car(line, &i);
          if (word) {
               list = erealloc(list, (++total + 1) * sizeof list);
               list[total - 1] = word;
               list[total] = NULL;
          }
     }
     return list;
}

/*
 * Get the first datum in a command line
 */
static char *cmdline_car(char *line, int *index)
{
     int i, j = 0, begin, end, quote_count = 0;
     char *retval;

     /* skip over leading whitespace */
     for (i = *index; whitespace(line[i]); i++)
          ;
     begin = i;
     /* now count the number of char's in word */
     while (line[i] != '\0' && !whitespace(line[i])) {
          /* skip over anything enclosed in double quotes */
          if (line[i] == '\"') {
               quote_count++;
               for (i++; line[i] && line[i] != '\"'; i++);
               if (line[i] == '\0') {
                    display(NORMAL, "Error: unfinished quote\n");
                    return NULL;
               } else if (line[i] == '\"')
                    quote_count++;
          }
          i++;
     }
     /* determine how much space will be needed to hold our
      * new string */
     end = i;
     if ((end - begin == 0) || (end - begin - quote_count == 0))
          return NULL;
     retval = emalloc(end - begin - quote_count + 1);
     /* copy over the string */
     i = begin;
     while(i < end) {
          if (line[i] != '\"')
               retval[j++] = line[i];
          i++;
     }
     retval[j] = '\0';
     *index = i;
     return retval;
}

/*
 * Strip out every thing on a line after 
 * the pipe character ('|').  
 */
static char *strippipe(char *string)
{
     int i;
     char *retval;

     assert(string);

     for (i = 0; string[i] && (string[i] != '|'); i++)
          ;
     retval = emalloc(i + 1);
     strncpy(retval, string, i);
     retval[i] = '\0';
     
     return retval;
}

/*
 * Find and return the remainder of a line
 * of ther the pipe ('|') character
 */
static char *pipe_scan(char *line)
{
     int i;
     char *retval;

     for (i = 0; line[i] && line[i] != '|'; i++)
          ;
     if (!i || i == strlen(line))
          return NULL;
     retval = emalloc(strlen(line) - i + 1);
     strcpy(retval, line + i + 1);
     
     return retval;
}

/*
 * this is a mock readline funciton.  if libreadline is
 * ever added, this will need to be removed.
 */
#define FBRL_BUFLEN 256
static char *readline(char *prompt)
{
     struct textlist_s {
          char *text;
          struct textlist_s *next;
     };
     char buffer[FBRL_BUFLEN];
     char *retval, *rtmp;
     size_t total_len = 0;
     struct textlist_s *list_head = NULL, *list_tail = NULL, *tmp;

     printf("%s", prompt);
     
     /* read all input into a list of buffers */
     do {
          tmp = emalloc(sizeof *tmp);
          fgets(buffer, FBRL_BUFLEN, stdin);
          tmp->text = strdup(buffer);
          tmp->next = NULL;
          if (!list_head)
               list_head = tmp;
          if (list_tail)
               list_tail->next = tmp;
          list_tail = tmp;
     } while (!ending(tmp->text));
     
     /* combine the list of buffers into
      * a single string
      */
     /* first calculate to total length. */
     for (tmp = list_head; tmp; tmp = tmp->next)
          total_len += tmp->next ? FBRL_BUFLEN - 1 : strlen(tmp->text) + 1;
     /* now create a single buffer */
     if (!total_len)
          return NULL;
     retval = emalloc(total_len);
     for (rtmp = retval, tmp = list_head; tmp; tmp = tmp->next, rtmp += FBRL_BUFLEN - 1)
          strcpy(rtmp, tmp->text);

     return retval;
}

/*
 * Determine if there exists a '\n' in the given buffer
 * also, convert any newlines to \0's if found.
 */
static int ending(char *buffer)
{
     while (*buffer) {
          if (*buffer == '\n') {
               *buffer = '\0';
               return 1;
          }
          buffer++;
     }
     return 0;
}

/* remove this when libncurses is added */
static int whitespace(char x)
{
     return (x == ' ' || x == '\t');
}
