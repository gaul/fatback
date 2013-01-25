/* $Id: recovery.c,v 1.2 2001/05/30 15:47:04 harbourn Exp $
 * File recovery module for fatback
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "recovery.h"
#include "dirtree.h"
#include "output.h"
#include "fat.h"
#include "input.h"
#include "util.h"
#include "output.h"

static int carve_file(clust_t *, unsigned long, unsigned long, unsigned long, int);
static int fname_is_avail(char *);

/* Error messages for the audit log */
char *log_notowner = "\"%s\": file may contain data that does not belong to it.\n";
char *log_clustoccupied = "\"%s\": cluster number %lu of file is occupied by an active file\n";
char *log_clustinvalid = "\"%s\": invalid cluster reported, contents may be innaccurate\n";
char *log_sizeinconsis = "\"%s\": File size inconsistancy, contents may be inaccurate\n";
char *log_nametaken = "\"%s\": name already taken, outputting cluster chain %lu to \"%s\" instead\n";
char *log_extracting = "Extracting cluster chain %lu to file %s\n";
char *log_carve = "\"%s\": Unable to recover file entirely,  carving instead.\n";
char *log_nocarve = "\"%s\": Unable to recover file entirely\n";
char *error_naming = "Error: Can't generate unique filename for \"%s\"\n";
char *error_reading = "Error: Unable to read cluster from input stream at location: %lu\n";
char *error_writing = "Error: Unable to write to file \"%s\"\n";

/*
 * Extract a file from the input stream, and place it in the file
 * specified by fname.  If size is specified as 0, then the entire
 * cluster chain is written out.
 */
int extract_file(
     dirent_t *ent,
     clust_t *clusts, 
     unsigned long bytes_per_clust,
     unsigned long size,
     unsigned long cluster,
     unsigned long max_cluster,
     char *filename)
{
     int file;
     char *fname;
     unsigned long clust = 0, left;
     void *buffer;
     unsigned long chainlen, reqd_clusts;

     assert(filename && clusts && bytes_per_clust);

     /* replace '~' with the users home directory */
     if (filename[0] == '~')
          filename = replace_tilde(filename);

     /* Find a filename for output */
     if (!(fname = unused_fname(filename))) {
          display(VERBOSE, error_naming, fname);
          free(fname);
          return -1;
     } else if (strcmp(fname, filename) != 0)
          display(VERBOSE, log_nametaken, filename, cluster, fname);

     if ((file = open(fname, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR)) == NULL) {
          perror("Error");
          free(fname);
          return -1;
     }
     /* If it is a directory entry being extracted, 
      * whose size is zero, just close the file */
     if (ent && !size) {
          close(file);
          free(fname);
          return 0;
     }
     
     display(VERBOSE, log_extracting, cluster,
           fname);

     chainlen = chain_length(clusts, cluster);
     reqd_clusts = size / bytes_per_clust;
     reqd_clusts += !!(size % bytes_per_clust);
     if (chainlen < reqd_clusts) {
          display(VERBOSE, log_carve, fname);
          carve_file(clusts, cluster, size, bytes_per_clust, file);
          return 0;
     }

     /* for efficiency we are going to use a single buffer of 
      * cluster size, and reuse it for each write */
     buffer = emalloc(bytes_per_clust);

     /* loop through the clusters, writing out the data */
     /* if a size of 0 was passed in, we are to recover the cluster chain */
     left = size ? size : chainlen * bytes_per_clust;
     do {
          size_t amount = left < bytes_per_clust? left : bytes_per_clust;
     
          clust = clust ? clusts[clust].fat_entry : cluster;
          if (!amount && size != 0) {
               display(VERBOSE, log_sizeinconsis, fname);
               close(file);
               free(buffer);
               free(fname);
               return -1;
          }
          if ((clust > max_cluster) || (clust < 2)) {
               display(VERBOSE, log_clustinvalid, fname);
               close(file);
               free(buffer);
               free(fname);
               return -1;
          }
          if (ent && ent != clusts[clust].owner) {
               if (clusts[clust].flags & CLUST_ACTIVE) {
                    display(VERBOSE, log_clustoccupied, fname, clust);
                    close(file);
                    free(buffer);
                    free(fname);
                    return -1;
               } else if (ent != NULL) {
                    /* keep track of which file this log message was last
                     * reported for to keep from having multiple log entries
                     * for the same file */
                    static dirent_t *last_notowner;
                    if (last_notowner != ent) {
                         display(VERBOSE, log_notowner, fname);
                         last_notowner = ent;
                    }
               }
          }
          errno = 0;
          read_data(buffer, clusts[clust].loc, bytes_per_clust);
          if (errno) {
               display(NORMAL, error_reading, clusts[clust].loc);
               close(file);
               free(buffer);
               free(fname);
               return -1;
          }
          if (amount != write(file, buffer, amount)) {
               display(NORMAL, error_writing, fname);
               close(file);
               free(buffer);
               free(fname);
               return -1;
          }
          /* mark the cluster as recovered */
          clusts[clust].flags |= CLUST_RECOVERED;
          left -= amount;
     } while (left && 
              !clust_is_end(&clusts[clust]) && 
              !clust_is_bad(&clusts[clust]) &&
              clusts[clust].fat_entry);

     close(file);
     free(buffer);
     free(fname);
     return 0;
}

/*
 * Extract clusters to a file.  This is a physical technique
 * because it does not check for other file's ownerships or
 * whether or not the file has been recovered. 
 */
static int carve_file(clust_t *clusts,
                      unsigned long cluster,
                      unsigned long size,
                      unsigned long bytes_per_clust,
                      int file)
{
     void *buffer;
     unsigned long left, ci;
     size_t amount;

     assert(bytes_per_clust && file && clusts);
     buffer = emalloc(bytes_per_clust);
     
     left = size;
     for (left = size, ci = 0; left != 0; left -= amount, ci++) {
          amount = left <  bytes_per_clust ? left : bytes_per_clust;
          read_data(buffer, clusts[cluster + ci].loc, bytes_per_clust);
          if (bytes_per_clust != write(file, buffer, amount))
               return -1;
     }
     return 0;
}
     
/*
 * Determine if a file in the real filesystem is a directory
 */
int stat_is_dir(char *arg)
{
     struct stat buf;
     
     if (0 > stat(arg, &buf)) {
          return -1;
     }
     return S_ISDIR(buf.st_mode);
}

/*
 * Concatenate a filename by adding a filename to the
 * end of a directory name.  Also append a '/' if necissary
 */
char *fn_cat(char *dir, char *file)
{
     int dir_len, file_len;
     char *retval;

     assert(dir && file);
     dir_len = strlen(dir);
     file_len = strlen(file);
     
     /* if the dir name already ends in '/'
      * remove it, for simplicity later */
     if (dir[dir_len - 1] == '/')
          dir[--dir_len] = '\0';

     /* add the directry name */
     retval = emalloc(dir_len + file_len + 2);
     strncpy(retval, dir, dir_len);
     retval[dir_len] = '/';
     /* now the file name */
     strncpy((retval + dir_len + 1), file, file_len);
     retval[dir_len + 1 + file_len] = '\0';
     return retval;
}

/*
 * Make a directory with a specified name
 */
int make_dir(char *dirname)
{
     assert(dirname);

     //dname = unused_fname(dirname);
     if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0) {
          perror("Error creating directory:");
          return -1;
     }
     return 1;
}


/*
 * Return a filename that is a unique permutation of
 * the fname passed in 
 */
char *unused_fname(char *fname)
{
     /* MAX_NUMBER_LEN represents the maximum length that
      * a printed representation of a number can be.  This
      * is arbitrarily picked.  Change this if there is a 
      * need for more than 10^MAX_NUMBER_LEN duplicate files.
      * The value of 100 should work. */
     const size_t MAX_NUMBER_LEN = 100;
     char *new_fname = fname;
     struct stat stat_buf;
     unsigned i;
     int test;

     assert(fname);

     /* the loop counter i will be the suffix whenever 
      * a unique name is found */
     for (i = 2; new_fname && !fname_is_avail(new_fname); i++) {
          size_t new_buflen = strlen(fname) + MAX_NUMBER_LEN + 1;
          if (new_fname != fname)
               free(new_fname);
          new_fname = emalloc(new_buflen);
          snprintf(new_fname, new_buflen, "%s.%u", fname, i);
     }

     return new_fname == fname ? strdup(fname) : new_fname;
}

/*
 * Determine if a filename is in use
 */
static int fname_is_avail(char *fname)
{
     struct stat stat_buf;

     if ((stat(fname, &stat_buf) < 0) && errno == ENOENT)
          return 1;
     else 
          return 0;
}

/*
 * Replace '~' with the users home directory in
 * a filename
 */
char *replace_tilde(char *fname)
{
     char *retval;
     char *homedir = getenv("HOME");

     retval = emalloc(strlen(homedir) + strlen(fname));

     strcpy(retval, homedir);
     strcat(retval, ++fname);
     
     return retval;
}
     
