/*********
 * $Id: cmd_stat.c,v 1.3 2001/05/30 15:47:03 harbourn Exp $
 * stat command for fatback
 *********/

#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include "interface_data.h"
#include "dirtree.h"
#include "output.h"

/*
 * Display file statistics for a 
 * file or directory.
 */
void cmd_stat(int argc, char *argv[])
{
     entlist_t *list, *ent; 

     if (argc < 2) {
          display(NORMAL, "Usage: stat [file] ...\n");
          return;
     }

     /* get a list of the files that the user specified */
     if (!(list = find_files(argc - 1, &argv[1]))) {
          display(NORMAL, "No files found\n");
          return;
     }

     /* display the stat informatin for each file */
     for (ent = list; ent; ent = ent->next) {
          display(NORMAL, "Filename: %s\n", ent->ent->filename);
          display(NORMAL, "Long Filename: %s\n", ent->ent->lfn);
          display(NORMAL, "Size: %lu\n", ent->ent->size);
          display(NORMAL, "Cluster: %lu\n", ent->ent->cluster);
          display(NORMAL, "Flags: %X\n", ent->ent->flags);
          display(NORMAL, "\n");
     }
     /* now free all the elements of the file list */
     ent = list;
     while (ent) {
          entlist_t *tmp = ent->next;
          free(ent);
          ent = tmp;
     }
}
