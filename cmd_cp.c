/***********
 * $Id: cmd_cp.c,v 1.8 2001/05/30 15:40:00 harbourn Exp $
 * cp command for fatback
 ***********/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "getopt.h"
#include "interface_data.h"
#include "interface.h"
#include "dirtree.h"
#include "recovery.h"
#include "output.h"

static struct option long_opts[] =
{
     {"deleted", 0, 0, 'd'},
     {"recursive", 0, 0, 'R'},
     {NULL, 0, 0, 0}
};

static char *usage = "Usage: cp -[dR] [files] [dir]\n   or: cp -[dR] [from file] [to file]\n";

static int contains_deleted(dirent_t *);

void cmd_cp(int argc, char *argv[])
{
     char *output_fname = NULL, *output;
     int bpc, to_dir;
     int opt, opt_index;
     int only_deleted = 0, recursive = 0;
     entlist_t *list, *ent, *ptr;

     if (argc < 3) {
          display(NORMAL, usage);
          return;
     }
     /* clear out getopt's globals, their might be stuff left over 
      * from the main() invocation of getopt_long. 
      */
     optarg = NULL;
     optind = 0;
     opterr = 0;
     optopt = 0;

     /* parse the command line options */
     while ((opt = getopt_long(argc, argv, "dR", long_opts, &opt_index)) > 0) {
          switch (opt) {
          case 'd':
               only_deleted = 1;
               break;
          case 'R':
               recursive = 1;
               break;
          default:
               break;
          }
     }
     if ((argc - optind) < 2) {
          display(NORMAL, usage);
          return;
     }
     output = argv[argc - 1];

     if (output[0] == '~')
          output = replace_tilde(output);

     to_dir = (stat_is_dir(output) > 0) ? 1 : 0;
     
     /* if their are more than one file being copied, and the 
      * givin output specifier is a file, warn the user. */
     if (!to_dir && !recursive) {
          display(NORMAL, "Error: last argument must be a directory\n");
          return;
     } else if (!to_dir && recursive) {
          char *new_output = unused_fname(output);
          if (!make_dir(new_output)) {
               free(new_output);
               return;
          }
          output = new_output;
          to_dir = 1;
     }

     if (!(list = find_files(argc - optind - 1, &argv[optind])))
          return;
     bpc = vbr->bytes_per_sect * vbr->sects_per_clust;
     /* now loop over all the files and write them out */
     for (ent = list; ent; ent = ent->next) {
          char *fname;
          if (ent->ent->lfn && (strlen(ent->ent->lfn) < NAME_MAX))
               fname = ent->ent->lfn;
          else
               fname = ent->ent->filename;
          if ((ent->ent->attrs & ATTR_DIR) && 
              (!only_deleted || contains_deleted(ent->ent))) {
               if (recursive) { 
                    /* recurse down the tree */
                    dirent_t *tmp = cwd;
                    char *myargv, *new_dirname = fn_cat(output, fname);
                    cwd = ent->ent;
                    myargv = argv[argc - 1];
                    argv[argc - 1] = new_dirname;
                    cmd_cp(argc, argv);
                    free(argv[argc - 1]);
                    argv[argc - 1] = myargv;
                    cwd = tmp;
               }
          }
          if (to_dir)
               output_fname = fn_cat(output, fname);
          else
               output_fname = output;
          if (!(only_deleted) || (ent->ent->flags & ENT_DELETED))
               extract_file(ent->ent,
                            clusts, 
                            bpc,
                            ent->ent->size,
                            ent->ent->cluster,
                            vbr->fat_entries,
                            output_fname);
          if (to_dir)
               free(output_fname);
     }
     /* now free all the elements of the list of entries */
     ptr = list;
     while (ptr) {
          entlist_t *tmp;
          tmp = ptr->next;
          free(ptr);
          ptr = tmp;
     }
}

/*
 * Recursively determine if a given directory 
 * contains any deleted files or is deleted itself.
 */
static int contains_deleted(dirent_t *dir)
{
     dirent_t *ent;

     if (dir->flags & ENT_DELETED)
          return 1;
     for (ent = dir->child; ent; ent = ent->next)
          if (ent->flags & ENT_DELETED)
               return 1;
     return 0;
}
