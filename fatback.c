/* $Id: fatback.c,v 1.24 2001/05/30 15:44:10 harbourn Exp $
 * At this point, this is just a test driver for
 * all of the various modules.  Hopefully someday, this will
 * be a real program (Grin).
 */
#ifdef VERSION
static char *fatback_version = VERSION;
#else
static char *fatback_version = "(Unknown)";
#endif /*VERSION*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "getopt.h"
#include <assert.h>
#include <string.h>
#include "input.h"
#include "output.h"
#include "mbr.h"
#include "util.h"
#include "vbr.h"
#include "fat.h"
#include "dirtree.h"
#include "interface.h"
#include "lfn.h"
#include "vars.h"

static void print_dirtree(dirent_t *) __attribute__ ((used));
static void print_help(char *);
static void print_ver(void);
static char *gen_opts(struct option *);
static char *str_cons(char *, char);

static char *output;  /* used in auto undelete mode */

enum { /* flags for undel_partition() */
     PART_ONLY   = 0x01,
     INTERACTIVE = 0x02
};

int main(int argc, char *argv[])
{
     int num_partitions;
     int flags = INTERACTIVE;
     int opt, opt_index;
     int partition = -1; 
     char *logname = NULL;
     int input_type = DFILE;
     char *opt_string;
     static struct option long_opts[] =
     {
          {"output", 1, 0, 'o'},
          {"log", 1, 0, 'l'},
          {"auto", 0, 0, 'a'},
          {"verbose", 0, 0, 'v'},
          {"partition", 1, 0, 'p'},
          {"delprefix", 1, 0, 'd'},
          {"single", 0, 0, 's'},
          {"sectsize", 1, 0, 'z'}, 
          {"help", 0, 0, 'h'},
          {"version", 0, 0, 'V'},
#ifdef HAVE_MMAP
          {"mmap", 0, 0, 'm'},
#endif /*HAVE_MMAP*/
          {0, 0, 0, 0}
     };

     if (argc == 1) {
          print_help(argv[0]);
          return 0;
     }

     /* set some default values */
     set_fbvar("loglevel", 5);
     set_fbvar("verbose", "off");
     set_fbvar("sectsize", 512);
     set_fbvar("prompt", "fatback>");
     set_fbvar("deleted_prefix", "?");

     /* generate an opt string based upon the long_opts[] array */
     opt_string = gen_opts(long_opts);

     /* parse the command line options */
     while ((opt = getopt_long(argc, argv, opt_string, long_opts, &opt_index)) > 0) {
          switch (opt) {
          case 'o':
               output = optarg;
               break;
          case 'l':
               logname = optarg;
               break;
          case 'a':
               flags &= !INTERACTIVE;
               break;
          case 'v':
               set_fbvar("verbose", "on");
               break;
          case 'p':
               partition = atoi(optarg);
               break;
          case 'd':
               set_fbvar("deleted_prefix", optarg);
               break;
          case 's':
               flags |= PART_ONLY;
               break;
          case 'z':
               set_fbvar("sectsize", atol(optarg));
               break;
          case 'h':
               print_help(argv[0]);
               return 0;
               break;
          case 'V':
               print_ver();
               return 0;
               break;
#ifdef HAVE_MMAP
          case 'm':
               input_type = MMAP;
               break;
#endif /*HAVE_MMAP*/
          default:
               break;
          }
     }

     /* Disable output buffering.  this isnt essential,
      * but for the '.' tic marks to show as the file system
      * is being parsed, it is essential. */
     setbuf(stdout, NULL);

     if (!argv[optind]) {
          printf("Error: No input file specified. Type '%s --help' for usage\n", argv[0]);
          return -1;
     }
     
     if (!(flags & INTERACTIVE) && !output) {
          printf("Auto undelete mode requires an output directory to be specified. (-o [dir])\n");
          return -1;
     }

     /* initialize audit logging and input handeling*/
     audit_init(logname, argv);
     if (input_init(input_type, argv[optind]) < 0)
          return -1;

     /* if the user forced a single partition, or if 
      * we are unable to find valid partition tables,
      * just jump into undeleteing a single partition
      */
     num_partitions = map_partitions();
     if (num_partitions == 0)
          flags |= PART_ONLY;
     if (partition < 0 && num_partitions < 2)
          partition = num_partitions;
     if (partition > num_partitions) {
          display(NORMAL, "Partition number specified is out of range,"
                  " only %d partitions detected\n", num_partitions);
          return -1;
     }

     if ((flags & INTERACTIVE) && partition < 0)
          partition_menu(num_partitions, flags);
     else
          undel_partition(partition, flags);

     /* if the user is in an interactive session, and
      * did we were able to find partition tables, 
      * then give them a menu, or jump to the partition
      * they specified on the command line.
      */

     /* wrap things up */
     input_close();
     audit_close();
     return 0;
}

/*
 * Process a single partition.  both interactively
 * and automatically.
 */
int undel_partition(int pnum, int flags)
{
     struct part_range_s *prange = NULL;
     vbr_t myvbr;
     unsigned long num_fat_entries;
     clust_t *clusts;
     off_t rdir_loc;
     dirent_t *root_dir;
     fbvar_t *verbose_var;
     unsigned verbose;

     /* we start out by building a vbr structure for 
      * this partition.
      */
     if (flags & INTERACTIVE) {
          printf("Parsing file system.\n");
          ticmarker();
     }

     if (flags & PART_ONLY) 
          myvbr = build_vbr(0);
     else {
          if ((prange = get_prange(pnum - 1)) == 0)
               return 0;
          myvbr = build_vbr(prange->start);
     }
     if (!myvbr)  
          return 0;

     /* audit log all information in the vbr */
     log_vbr(myvbr);

     /* quit this partition if it is not a FAT fs */
     switch (get_fs_type(myvbr)) {
     case VBR_FAT12:
     case VBR_FAT16:
     case VBR_FAT32:
          break;
     default:
          return 0;
     }

     /* find out where our root directory is as well as build a
      * virtual image of the FAT table */
     ticmarker();

     if (flags & PART_ONLY) {
          rdir_loc = get_root_loc(0, myvbr);
          clusts = build_fat(&num_fat_entries, 0, myvbr);
     } else {
          rdir_loc = get_root_loc(prange->start, myvbr);
          clusts = build_fat(&num_fat_entries, prange->start, myvbr);
     }
     myvbr->fat_entries = num_fat_entries;
     display(VERBOSE, "Rood dir location: %lu\n", rdir_loc); 
     if (!clusts) {
          display(VERBOSE, "Unable to read FAT in partition\n");
          return 0;
     }
     ticmarker();

     /* create a directory entry stucture for the root and
      * fill it with the essential data */
     root_dir = emalloc(sizeof *root_dir);
     root_dir->parent = NULL;
     root_dir->child = NULL;
     root_dir->next = NULL;
     root_dir->lfn_list = NULL;
     root_dir->lfn = NULL;
     root_dir->filename = "root";
     root_dir->attrs |= ATTR_DIR;
     root_dir->child = build_tree(clusts, 
                                  num_fat_entries,
                                  rdir_loc, myvbr,
                                  root_dir);
     ticmarker();

     /* piece together all long file name fragments and 
      * associate them with there corresponding directory
      * entries. 
      */
     cat_lfn_tree(root_dir);
     lfn_assoc_tree(root_dir);
     unichoke_tree(root_dir);
     ticmarker();
     printf(" (Done)\n");

     /* if the session is interactive, then just jump to the
      * command interpreter.  However, if its automatic mode,
      * then build and execute fixed commands. */
     interface_init(root_dir, clusts, myvbr);
     if (flags & INTERACTIVE)
          return process_commands();
     else {
          char *cmd_first = "cp -d -R *";  /* the "canned" command. it 
                                            * is the copy command with
                                            * the only-deleted and 
                                            * recursive flags on. */
          char *cmd_total;
          cmd_total = emalloc(strlen(cmd_first) + strlen(output) + 1);
          /* add the output directory to the command line. */
          sprintf(cmd_total, "%s %s", cmd_first, output);
          exec_line(cmd_total);
          return 0;
     }
}

/*
 * print out a directory tree.
 * only used in debugging
 */
static void print_dirtree(dirent_t *ent_list)
{
     dirent_t *ent;
     for (ent = ent_list; ent; ent = ent->next) {
          printf("%s\n", ent->lfn ? ent->lfn : ent->filename);
          if (ent->child)
               print_dirtree(ent->child);
     }
}

/*
 * Display the help screen
 */
static void print_help(char *cmd)
{
     printf("Usage: %s [FILE] -l [LOG] [OPTION]...\n"
            "Undelete files from FAT filesystems.\n", cmd);
     printf("Fatback v"); print_ver();
     printf("(c) 2000-2001 DoD Computer Forensics Lab\n");
     printf("  -o, --output=DIR          specifies a directory to place output files\n"
            "  -a, --auto                auto undelete mode. non-interactively\n"
            "                              recovers all deleted files\n"
            "  -l, --log=LOGFILE         specifies a file to audit log to.\n"
            "  -v, --verbose             display extra information to the screen.\n"
            "  -p, --partition=PNUM      go directly to PNUM partition\n"
            "  -d, --delprefix=PREFIX    use PREFIX to signify deleted files instead\n"
            "                              of the default \"?\"\n"
            "  -s, --single              force into single partition mode\n"
            "  -z, --sectsize=SIZE       adjust the sector size. default is 512\n"
#ifdef HAVE_MMAP
            "  -m, --mmap                use mmap() file I/O for improved performance\n"
#endif /*HAVE_MMAP*/
            "  -h, --help                display this help screen\n"
            "Report bugs to <harbourn@dcfl.gov>\n");
}

/*
 * Display the program version
 */
static void print_ver(void)
{
     printf("%s\n", fatback_version);
}

/*
 * Generate an option string for the getopt() function
 * given an array of stuct option's
 */
static char *gen_opts(struct option *options)
{
     char *retval = NULL;
     int i;

     assert(options);
     /* loop through all the structures in options[] */
     for (i = 0; options[i].val != '\0'; i++) {
          if (options[i].val) {
               char *tmp;
               /* create the string retval if needed */
               if (!retval) {
                    retval = emalloc(1);
                    *retval = '\0';
               }
               /* create a new string that is the old
                * string plus the new character */
               tmp = retval;
               retval = str_cons(retval, options[i].val);
               free(tmp);
           
               /* if the option takes an argument, add
                * the ':' character to the string */
               if (options[i].has_arg) {
                    tmp = retval;
                    retval = str_cons(retval, ':');
                    free(tmp);
               }
          }
     }

     return retval;
}

/* 
 * Create a new string that is a concatenation of
 * str and chr.
 */
static char *str_cons(char *str, char chr)
{
     char *retval = NULL;
     int len;
     
     assert(str);
     
     len = strlen(str);
     retval = emalloc(len + 2);
     strcpy(retval, str);
     
     retval[len] = chr;
     retval[len + 1] = '\0';

     return retval;
}
