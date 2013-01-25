/***********
 * $Id: cmd_cpchain.c,v 1.4 2001/05/30 15:40:55 harbourn Exp $
 * cpchain command for fatback
 ***********/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"
#include "interface_data.h"
#include "fat.h"
#include "output.h"
#include "util.h"
#include "recovery.h"

/*
 * Copy a cluster chain to a file.
 */
void cmd_cpchain(int argc, char *argv[])
{
     char *default_name, *name, *def_prefix = "chain-";
     unsigned long cluster;
     int bpc;

     if (argc < 3 || !(cluster = atol(argv[1]))) {
	  display(NORMAL, "Usage: cpchain cluster [output file]\n");
	  return;
     }
     /* calculate the bytes per sector */
     bpc = vbr->bytes_per_sect * vbr->sects_per_clust;

     /* generate a default name, incase the user only
      * specifies a directory to put the file */
     default_name = emalloc(strlen(def_prefix) + strlen(argv[1]) + 1);
     strcpy(default_name, def_prefix);
     strcpy(default_name + strlen(def_prefix), argv[1]);
     if (!argv[2])
          name = default_name;
     else {
          name = argv[2];
          if (*argv[0] == '~')
               name = replace_tilde(name);
          if (stat_is_dir(name) > 1) 
               name = fn_cat(name, default_name);
     }

     extract_file(NULL, clusts, bpc, 0, cluster, vbr->fat_entries, name);
     free(default_name);
     return;
}
