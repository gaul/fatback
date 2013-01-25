/****************
 * $Id: dirtree.c,v 1.14 2001/05/30 15:47:03 harbourn Exp $
 * This module deals with reading directories and entries
 ****************
 */

#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "fat.h"
#include "util.h"
#include "input.h"
#include "output.h"
#include "dirtree.h"
#include "lfn.h"
#include "vars.h"

static dirent_t *parse_entry(u_int8_t *, unsigned);
static char *cat_filename(char *, char *);
static void parse_time(struct tm *, u_int8_t *);
static int scheck_dirent(dirent_t *);
static int scheck_filename(char *);
static int is_legal_fnchar(char);
static int is_blank_entry(u_int8_t *);
static unsigned long convert_time(struct tm *);
static int allocate_chain(dirent_t *, clust_t *, unsigned long, unsigned long);

/*
 * Recursively build a directory tree. also flags clusters as
 * necessary and recursively parses deleted directories.
 */
dirent_t *build_tree(clust_t *clusts,
                     unsigned long num_clusts,
                     unsigned long dir_clust, 
                     vbr_t vbr,
                     dirent_t *parent)
{
     dirent_t *ent_list, *ent;

     assert(clusts);
     assert(vbr);
     if (!(ent_list = build_dir(clusts, dir_clust, vbr, parent)))
          return NULL;
     /* now go through the list and recurse down any subdirectory */
     for (ent = ent_list; ent; ent = ent->next) {
          ent->parent = parent;
          /* go through and mark its clusters as used */
          if ((allocate_chain(ent, clusts, num_clusts, ent->cluster)) &&
              (ent->attrs & ATTR_DIR) && (ent->size == 0)) {
               /* recurse if needed */
               ent->child = build_tree(clusts, num_clusts, 
                                       ent->cluster, vbr, ent);
          }
     }
     return ent_list;
}    
     
/*
 * Reads in a directory at the given offset, returns a list of
 * entries
 */
dirent_t *build_dir(clust_t *clusts,
                    unsigned long dir_clust, 
                    vbr_t vbr,
                    dirent_t *parent)
{
     clust_t *clust = NULL;
     unsigned long clust_size;
     dirent_t *ent_list = NULL, *ent_list_tail = NULL;
     unsigned seq_no = 0;
     unsigned long max_clust;
     int end_of_dir = 0; /* when the logical dir is finished this
                          * switches to 1, that way entries after that
                          * get flagged as deleted */
     const int ENT_SIZE = 32; /* the size of a dir entry */
     const char SIGMA = '\xE5'; /* The marker of a deleted file */
     fbvar_t *delprefix_var;
     char *delprefix;

     assert(clusts);
     assert(vbr);
     if (!vbr->bytes_per_sect || vbr->bytes_per_sect % 32) {
          display(VERBOSE, "bytes per sector is invalid, cannot proceed\n");
          return NULL;
     }
     
     ticmarker();
     /* retrieve the string for the prefix of deleted entries */
     delprefix_var = get_fbvar("deleted_prefix");
     delprefix = delprefix_var->val.sval;
     free(delprefix_var);

     if (dir_clust) 
          clust_size = vbr->bytes_per_sect * vbr->sects_per_clust;
     else 
          clust_size = vbr->max_rdir_entries * ENT_SIZE;
     max_clust = vbr->total_sects_l ? vbr->total_sects_l : vbr->total_sects_s;
     max_clust /= vbr->sects_per_clust;
     /* loop through each cluster in the chain */
     do {
          u_int8_t *bptr, *buf = emalloc(clust_size);
          off_t loc;

          clust = clust ? &clusts[clust->fat_entry] : &clusts[dir_clust];
          loc = dir_clust ? clust->loc : vbr->d.fat1216.root_dir_loc;
          if (!read_data(buf, loc, clust_size))
               return NULL;
          for (bptr = buf; bptr < buf + clust_size; seq_no++, bptr += ENT_SIZE)
          {
               dirent_t *ent;
               /* skip over the beginning . and .. entries */
               if ((seq_no < 2) && ('.' == (char)buf[0]))
                    continue;

               ent = parse_entry(bptr, seq_no);
               /* one last little sanity check */
               if (ent && ent->cluster > max_clust) {
                    free(ent);
                    ent = NULL;
               }
               if (ent) {
                    char *original_fname = ent->filename;
                    if (ent->filename[0] == SIGMA)
                         original_fname++;                  
                    if (end_of_dir || ent->filename[0] == SIGMA) {
                         int newlen = strlen(delprefix) + strlen(original_fname) + 1;
                         int i;
                         ent->flags |= ENT_DELETED;
                         /* add the appropriate prefix */
                         ent->filename = emalloc(newlen);
                         for (i = 0; i < newlen; i++) 
                              ent->filename[i] = '\0';
                         strcat(ent->filename, delprefix);
                         strcat(ent->filename, original_fname);
                    }               
                    /* add it to the list of entries */
                    if (!ent_list) 
                         ent_list_tail = ent_list = ent;
                    else 
                         ent_list_tail = ent_list_tail->next = ent;
               } else if (is_blank_entry(bptr)) {
                    end_of_dir = 1;
               } else {
                    /* if its not a dirent and still part of the
                     *  active directory, try and make it a long 
                     * filename fragment */
                    lfn_t *frag = parse_lfn(bptr);
                    if (frag) {
                         frag->dir_seq_num = seq_no;
                         frag->dir = parent;
                         frag->next = parent ? parent->lfn_list : NULL;
                         parent->lfn_list = frag;
                    } else {
                         static clust_t *lastlogged_clust;
                         /* any directory data that is left at this
                          * point can only be garbage. */
                         if (lastlogged_clust != clust) {
                              display(VERBOSE, "Unrecognized directory information in cluster at offset %lu\n", clust->loc);
                              lastlogged_clust = clust;
                         }
                    }
               }             
          }
          free(buf);
     } while (dir_clust && clust->fat_entry >= 2 && !clust_is_end(clust));
     return ent_list;
}

/*
 * Determine if entry A is newer than entry B
 */
int is_newer(dirent_t *a, dirent_t *b)
{
     /* the easiest way to do this is to total up
      * a and b's times into the number of seconds
      * since 1 Jan 1980 (not exactly) and return 
      * the comparison */

     return convert_time(&a->time) > convert_time(&b->time);
}

/*
 * Determine if a character is a legal character for a filename
 */
static int is_legal_fnchar(char ch)
{
     static const char *chars = 
          "\xE5\x05.ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789~!@#$^&()-{}'";
     int i;
     for (i = 0; chars[i]; i++)
          if (chars[i] == ch)
               return 1;
     return 0;
}
     
/*
 * Converts a time structure (struct tm) into
 * a single value. Note: we couldent use the normal
 * time structure conversion functions because 
 * we dont fill in all of the fields in struct tm.
 */
static unsigned long convert_time(struct tm *time)
{
     unsigned long value;

     value = time->tm_year;
     value *= 12;
     value += time->tm_mon;
     value *= 31;
     value += time->tm_mday;
     value *= 24;
     value += time->tm_hour;
     value *= 60;
     value += time->tm_min;
     value *= 60;
     value += time->tm_sec;
     
     return value;
}
     
/*
 * Parse the data at a givin offset into a directory entry
 * structure.  
 */
static dirent_t *parse_entry(u_int8_t *buf, unsigned seq_no)
{
     dirent_t *entry;
     char *fname, *ext;
     enum { FNAME_LEN    = 8,
            EXT_LEN      = 3,
            EXT_OFF      = 8,
            ATTRIBS_OFF  = 11,
            HICLUST_OFF  = 20,
            TIME_OFF     = 22,
            CLUST_OFF    = 26,
            SIZE_OFF     = 28
     };

     assert(buf);

     entry = emalloc(sizeof *entry);
     
     /* read the filename into a string */
     /* this gets a little tricky because we only want
      * the . and extension to show up if there is an
      * extension present. 
      */
     fname = emalloc(FNAME_LEN + 1); /* add for terminator*/
     strncpy(fname, buf, FNAME_LEN);
     fname[FNAME_LEN] = '\0';

     ext = emalloc(EXT_LEN + 1); 
     strncpy(ext, &buf[EXT_OFF], EXT_LEN);
     ext[EXT_LEN] = '\0';
     /* add the first 8 and the last 3 together with a . if needed */
     entry->filename = cat_filename(fname, ext);

     entry->attrs = little_endian_8(buf + ATTRIBS_OFF);
     parse_time(&entry->time, buf + TIME_OFF);
     entry->cluster = little_endian_16(buf + HICLUST_OFF) << 16;
     entry->cluster += little_endian_16(buf + CLUST_OFF);
     entry->size = little_endian_32(buf + SIZE_OFF);
     entry->sequence_num = seq_no;
     entry->parent = NULL;
     entry->child = NULL;
     entry->next = NULL;
     entry->flags = 0;
     entry->lfn = NULL;
     entry->lfn_list = NULL;
     if (scheck_dirent(entry))
          return entry;
     else {
          free(entry);
          return NULL;
     }
}

/*
 * Take to pieces of a filename (8 and 3) and form
 * a proper UNIX like filename out of them.  in other
 * words, jam them together with a . seperator, and if
 * there is no extension, dont put a . in there at all
 */     
static char *cat_filename(char *fname, char *ext)
{
     enum { FILENAME_LEN = 12 };
     char *filename = emalloc(FILENAME_LEN + 1);
     int i = 0, j = 0;
  
     assert(fname);
     assert(ext);
     /* start out by copying fname into filename */
     /* cant use strncpy because we want to stop
      * at whitespace or \0 */
     while (is_legal_fnchar(fname[i]) && (filename[i] = fname[i]))
          i++;
     
     /* now count the number of non-whitespace 
      * characters in the extension */
     while (ext[j] && is_legal_fnchar(ext[j]))
          j++;
     if (j) {
          int k = 0;
          filename[i++] = '.';
          /* now add the extension to the end */
          while (is_legal_fnchar(ext[k]) && (filename[i++] = ext[k]))
               k++;
     }
     filename[i] = '\0';
     return filename;
}

/*
 * Parse a DOS time field.
 * data is moved into a struct tm, however this will
 * be incomplete.  not all fields will be filled.
 */
static void parse_time(struct tm *time, u_int8_t *buf)
{
     unsigned long value;
     enum { HOURS_MASK   = 0xF8000000L,
            MINUTES_MASK = 0x07E00000L,
            SECONDS_MASK = 0x001F0000L,
            YEAR_MASK    = 0x0000FE00L,
            MONTH_MASK   = 0x000001E0L,
            DAY_MASK     = 0x0000001FL,
            /* these magnitudes represent how many 
             * bits the values need to be shifted by*/
            HOURS_MAG   = 32 - 5,
            MINUTES_MAG = HOURS_MAG - 6,
            SECONDS_MAG = MINUTES_MAG - 5,
            YEAR_MAG = SECONDS_MAG - 7,
            MONTH_MAG = YEAR_MAG - 4,
            DAY_MAG  = MONTH_MAG - 5
     };

     assert(time);
     assert(buf);

     /* get the buffer data into format that we can
      * play with very easily, such as a number!
      */
     value = little_endian_16(buf) << 16;
     value += little_endian_16(buf+2);

     /* now fill the time structure with all of the 
      * info that we can extrapolate from the data
      * we just pulled out of the buffer
      */
     time->tm_hour = (value & HOURS_MASK) >> HOURS_MAG;
     time->tm_min = (value & MINUTES_MASK) >> MINUTES_MAG;
     time->tm_sec = (value & SECONDS_MASK) >> SECONDS_MAG;
     time->tm_sec *= 2;
     time->tm_year = (value & YEAR_MASK) >> YEAR_MAG;
     time->tm_year += 80;  /* DOS years start at 1980 */
     time->tm_mon = (value & MONTH_MASK) >> MONTH_MAG;
     time->tm_mon -= 1;
     time->tm_mday = (value & DAY_MASK) >> DAY_MAG;
}

/*
 * Sanity check a directory entry
 */
static int scheck_dirent(dirent_t *entry)
{
     /* first, we should make sure there is at least
      * one legal non-whitespace character in the first
      * part of the filename
      */ 
     assert(entry);
     if (!scheck_filename(entry->filename))
          return 0;
     /* see if the cluster number looks reasonable */
     {
          unsigned long cn = entry->cluster;
          const unsigned long maxclust = 0x0FFFFFFFL;
          if (cn==1 || cn > maxclust)
               return 0;
     }
     /* ok, now we should check out the time stamp
      * and make sure it makes sense.
      */
     if (entry->time.tm_hour > 23 ||
         entry->time.tm_min > 59 ||
         entry->time.tm_sec > 61 ||
         entry->time.tm_mon > 11 ||
         entry->time.tm_mday == 0)
          return 0;
     /* make sure the attributes are sane */
     if ((entry->attrs & ATTR_RESERVE) ||
         ( (entry->attrs & ATTR_VOLUME) &&
           ( (entry->attrs & ATTR_RO) ||
             (entry->attrs & ATTR_HIDDEN) ||
             (entry->attrs & ATTR_SYSTEM) ||
             (entry->attrs & ATTR_DIR))))
          return 0;

     /* If we have made it here, all must be well */
     return 1;
}

/*
 * Sanity check a filename
 */
static int scheck_filename(char *fn)
{                           
     /* note: this code will not work on systems that do not
      * use the ASCII character coding scheme. */
     int i, j, inval = 0, dotcount = 0;
     /* first off, make sure that the there is some thing
      * in the first part of the filename */
     for (i=0; !inval && i < 8 && fn[i] && fn[i] != '.'; i++)       
          if (!is_legal_fnchar(fn[i]))  
               inval++;            
     if (!i)
          inval++;
     /* now check the validity of the extension, also make
      * sure there isnt more than one '.' */
     j = i;
     for (i=0; !inval && i < 4 && fn[j+i]; i++)
          if (!is_legal_fnchar(fn[j+i]))
               j++;
          else if (fn[j+i] == '.')
               dotcount++;
     if (dotcount > 1)
          inval++;
     return inval ? 0 : 1;
}

/*
 * Determine if a directory entry is blank.
 * a blank entry is a way of signifiing (sp?)
 * the end of a directory.
 */
static int is_blank_entry(u_int8_t *buf)
{
     /* if the first two bytes are zero's, its blank */
     if (!buf[0] && !buf[1])
          return 1;
     else 
          return 0;
}

/*
 * Add an entry into the cluster map, or not, depending on 
 * if the entry is newer than what the cluster map says is
 * already using that cluster
 */
static int allocate_chain(dirent_t *ent, clust_t *clusts, 
                          unsigned long num_clusts, 
                          unsigned long cnum)
{
     /* I am initially going to implement this recursively,
      * this will be easy to code, but the program may
      * be a memory hog when processing large files. If
      * memory is ever a huge issue, rewrite this in a 
      * non recursive way */
     if (clust_is_bad(&clusts[cnum]) ||
         clust_is_resvd(&clusts[cnum]) ||
         cnum > num_clusts ||
         cnum < 2)
          return 0;
     if (!(ent->flags & ENT_DELETED)) { 
          if ((clusts[cnum].flags == CLUST_FREE) ||
              (clusts[cnum].flags & CLUST_DELETED)) {
           
               /* set the new flag and remove the old ones */
               clusts[cnum].flags = CLUST_ACTIVE;
               clusts[cnum].flags &= ~CLUST_DELETED;
           
               clusts[cnum].owner = ent;
          } else if (is_newer(ent, clusts[cnum].owner))
               /* this shouldnt happen in a sane filesystem*/
               clusts[cnum].owner = ent;
     } else {  /* if its deleted */
          if (clusts[cnum].flags == CLUST_FREE) {
               clusts[cnum].owner = ent;
               clusts[cnum].flags |= CLUST_DELETED;
          } else if (clusts[cnum].flags & CLUST_DELETED) {
               /* if another deleted file claims this cluster,
        * the newer of the two wins. */
               if (is_newer(ent, clusts[cnum].owner))
                    clusts[cnum].owner = ent;
          }
     }
     /* if the cluster was successfully allocated to this entry, 
      * then recurse down the chain, otherwise return 0 */
     if (clusts[cnum].owner == ent) {
          if (!clust_is_end(&clusts[cnum]))
               return allocate_chain(ent, clusts, num_clusts, clusts[cnum].fat_entry);
          else 
               return 1;
     } else
          return 0;
}
