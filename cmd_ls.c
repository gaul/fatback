/***********
 * $Id: cmd_ls.c,v 1.6 2001/05/30 15:42:01 harbourn Exp $
 * ls command for fatback
 ***********/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "interface.h"
#include "interface_data.h"
#include "dirtree.h"
#include "output.h"

static void list_dir(dirent_t *);
static void list_ent(dirent_t *);

/*
 * The ls command lists files in a directory
 */
void cmd_ls(int argc, char *argv[])
{
     entlist_t *list, *ent;

     /* if no argument was passed, then call ourself
      * again with the "." argument to display the 
      * current directory */
     if (argc < 2) {
          list_dir(cwd);
          return;
     }

     if (!(list = find_files(argc - 1, &argv[1]))) {
          return;
     }

     /* first display all non directory entries */
     for (ent = list; ent; ent = ent->next)
          if (!(ent->ent->attrs & ATTR_DIR))
               list_ent(ent->ent);

     /* now display the contents of any directories */
     for (ent = list; ent; ent = ent->next)
          if (ent->ent->attrs & ATTR_DIR) {
               char *dirname = ent->ent->lfn ? ent->ent->lfn : ent->ent->filename;
               display(NORMAL, "%s:\n", dirname);
               list_dir(ent->ent);
          }
     
     /* now free all the list of files from find_files() */
     while (list) {
          entlist_t *tmp = list->next;
          free(list);
          list = tmp;
     }  
}

/*
 * List the files in a given directory
 */
static void list_dir(dirent_t *dir)
{
     dirent_t *ent;

     assert(dir);
     for (ent = dir->child; ent; ent = ent->next)
          list_ent(ent);
}

/*
 * Display a single directory entry
 */
static void list_ent(dirent_t *ent)
{
     int offset = 15; /* this is to keep the lfn's printing on the same column. */
     char *datestring;

     assert(ent);
     datestring = asctime(&ent->time);
     if (strlen(datestring) > 0) {
          datestring[strlen(datestring) - 1] = '\0';
          display(NORMAL, "%s ", datestring);
     }
     display(NORMAL, "%10lu", ent->size);
     offset -= display(NORMAL, " %.12s", ent->filename);
     offset -= display(NORMAL, "%c", (ent->attrs & ATTR_DIR)? '/' : ' ');
     if (ent->lfn) {
          while (offset--)
               putchar(' ');
          display(NORMAL, "%s", ent->lfn);
     }
     display(NORMAL, "\n");
}
