/*********
 * $Id: cmd_chain.c,v 1.4 2001/05/30 15:38:25 harbourn Exp $
 * chain command for fatback
 *********/

#include <stdio.h>
#include <stdlib.h>
#include "interface.h"
#include "interface_data.h"
#include "dirtree.h"
#include "output.h"
#include "fat.h"

/*
 * The chain command to display the cluster
 * chain of a directory entry.
 */
void cmd_chain(int argc, char *argv[])
{
     entlist_t *list, *ent;
     
     if (argc < 2) {
          display(NORMAL, "Usage: chain [file] ...\n");
          return;
     }
     
     /* get a list of files the user specified */
     if (!(list = find_files(argc - 1, &argv[1]))) {
          display(NORMAL, "No files found\n");
          return;
     }

     /* loop through the list of files, displaying 
      * cluster chains for each */
     for (ent = list; ent; ent = ent->next) {
          unsigned long clust = 0;
          dirent_t *dent = ent->ent;
          char *fn = dent->lfn ? dent->lfn : dent->filename;
          display(NORMAL, "cluster chain for \"%s\"\n", fn);
          do {
               clust = clust ? clusts[clust].fat_entry : dent->cluster;
               display(NORMAL, "%10lu", clust);
          } while (clust >= 2 &&
                   !clust_is_end(&clusts[clust]) &&
                   !clust_is_bad(&clusts[clust]));
          display(NORMAL, "\n");
     }
     ent = list;
     while (ent) {
          entlist_t *tmp = ent->next;
          free(ent);
          ent = tmp;
     }
}
