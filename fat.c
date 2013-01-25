/***************
 * $Id: fat.c,v 1.8 2001/05/30 15:47:03 harbourn Exp $
 * File allocation table processing module for fatback
 ***************
 */

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include "vbr.h"
#include "output.h"
#include "util.h"
#include "input.h"
#include "fat.h"

static clust_t *read_fat12(unsigned long *, off_t, int);
static unsigned fat12_first_half(u_int8_t *);
static unsigned fat12_last_half(u_int8_t *);
static clust_t *read_fat16(unsigned long *, off_t, int);
static clust_t *read_fat32(unsigned long *, off_t, int);

/* these constants describe the conditions for clusters */
static const unsigned long RESERVED = 0xFFFFFF0L;
static const unsigned long BAD = 0xFFFFFF7L;
static const unsigned long LAST = 0xFFFFFFFL;

/*
 * The main interface to this module.  it sets up proper 
 * values based on the vbr and calls read_fat[12,16,32] to do the
 * actual loading of the values into the array. the offset
 * parameter is the offset of the beginning of the partition
 */
clust_t *build_fat(unsigned long *entries, off_t offset, vbr_t vbr)
{
     unsigned long fat_size;
     clust_t *clust_array;
     clust_t *(*read_fat)(unsigned long *,off_t,int);
     const int DIRENT_SIZE = 32;

     assert(entries);
     assert(vbr);
     fat_size = vbr->bytes_per_sect;
     fat_size *= vbr->sects_per_fat;

     offset += vbr->reserved_sects * vbr->bytes_per_sect;

     switch (get_fs_type(vbr)) {
     case VBR_FAT12:
          read_fat = &read_fat12;
          break;
     case VBR_FAT16:
          read_fat = &read_fat16;
          break;
     case VBR_FAT32:
          read_fat = &read_fat32;
          break;
     default:
          read_fat = NULL;
     }
     if (!read_fat) {
          display(NORMAL, "No valid fat filesystem found\n");
          return NULL;
     }
     if (!(clust_array = (*read_fat)(entries, offset, fat_size))) {
          display(NORMAL, "Unable to read fat filesystem at offset %l\n", offset);
          return NULL;
     }

     /* Now fill the clusters structs with their offsets 
      * and other information */
     {
          unsigned long i;
          off_t c2_off;  /* The offset of the beginning cluster */
          unsigned cluster_size; 
      
          cluster_size = vbr->bytes_per_sect * vbr->sects_per_clust;
          c2_off = (vbr->fat_copies * vbr->sects_per_fat);
          c2_off *= vbr->bytes_per_sect;
          c2_off += vbr->max_rdir_entries * DIRENT_SIZE;
          c2_off += offset;
          for (i = 0; i < *entries; i++) {
               if (i < 2)
                    continue;
               clust_array[i].loc = c2_off + ((i-2) * cluster_size);
               /* all clusters are assumed to be free in
                * the beginning, so lets flag them as such */
               clust_array[i].flags = CLUST_FREE;
               clust_array[i].owner = NULL;
          }
     }
     return clust_array;
}

/*
 * These functions provide an abstract way to tell if a FAT entry
 * is reserved, bad, or the end of a chain
 */
int clust_is_resvd(clust_t *entry)
{
     return entry->fat_entry == RESERVED;
}

int clust_is_bad(clust_t *entry)
{
     return entry->fat_entry == BAD;
}

int clust_is_end(clust_t *entry)
{
     return entry->fat_entry == LAST;
}


/*
 * Marks the clusters in the givin 
 * cluster chain with a flag
 */
void flag_chain(clust_t *clusts, unsigned long cnum, u_int8_t flag)
{
     /* This function should not be used to flag something as 
      * deleted, because it doesnt check the existing flag */
     do clusts[cnum].flags = flag;
     while (!clust_is_end(&clusts[cnum]) 
            && (cnum = clusts[cnum].fat_entry));
}

/*
 * Determine how long a chain is (in number of clusters)
 */
unsigned long chain_length(clust_t *clusts, unsigned long chain)
{
     unsigned long retval = 1, cnum = chain;
     
     if (chain < 2)
          return 0;

     while (!clust_is_end(&clusts[cnum]) && 
            !clust_is_bad(&clusts[cnum]) &&
            !clust_is_resvd(&clusts[cnum])) {
          cnum = clusts[cnum].fat_entry;
          retval++;
     }
     return retval;
}

/*
 * Reads in the 12bit File Allocation table and puts its values into
 * an array.  it returns that array.
 */
static clust_t *
read_fat12(unsigned long *entries, off_t offset, int buf_size)
{
     int i, fat_index = 0;
     u_int8_t *buffer;
     clust_t *fat_array;
     /* See the note in read_fat16() for an explaination of these
      * constants
      */
     const int RESERVED_MIN = 0xFF0;
     const int RESERVED_MAX = 0xFF6;
     const int BAD12 = 0xFF7;
     const int LAST_MIN = 0xFF8;
     const int LAST_MAX = 0xFFF;

     assert(entries);
     if (!buf_size)
          return NULL;
     
     *entries = (buf_size * 2) / 3;
     fat_array = emalloc(*entries * sizeof *fat_array);
     buffer = emalloc(buf_size);
     if (!read_data(buffer, offset, buf_size))
          return NULL;

     for (i = 0; i < buf_size; i += 3) {
          fat_array[fat_index].fat_entry = fat12_first_half(buffer + i);
          if ((RESERVED_MIN <= fat_array[fat_index].fat_entry) &&  
              (fat_array[fat_index].fat_entry <= RESERVED_MAX))
               fat_array[fat_index].fat_entry = RESERVED;
          else if ((LAST_MIN <= fat_array[fat_index].fat_entry) &&
                   (fat_array[fat_index].fat_entry <= LAST_MAX))
               fat_array[fat_index].fat_entry = LAST;
          else if (fat_array[fat_index].fat_entry == BAD12)
               fat_array[fat_index].fat_entry = BAD;
          fat_index++;
          fat_array[fat_index].fat_entry = fat12_last_half(buffer + i);
          if ((RESERVED_MIN <= fat_array[fat_index].fat_entry) &&  
              (fat_array[fat_index].fat_entry <= RESERVED_MAX))
               fat_array[fat_index].fat_entry = RESERVED;
          else if ((LAST_MIN <= fat_array[fat_index].fat_entry) &&
                   (fat_array[fat_index].fat_entry <= LAST_MAX))
               fat_array[fat_index].fat_entry = LAST;
          else if (fat_array[fat_index].fat_entry == BAD12)
               fat_array[fat_index].fat_entry = BAD;
          fat_index++;
     }
     
     free(buffer);
     return fat_array;
}
     
/*
 * Take a pointer to 24 bits of data and extract the first 12
 * bits of data stored in little endian form.
 */
static unsigned fat12_first_half(u_int8_t *buf)
{
     unsigned retval;
     static const int FIRST_HALF_MASK = 0x0F;

     assert(buf);
     retval = buf[0];
     retval += (buf[1] & FIRST_HALF_MASK) << 8;
     return retval;
}

/*
 * Take a pointer to 24 bits of data and extract the last 12
 * bits of data stored in little endian form.
 */
static unsigned fat12_last_half(u_int8_t *buf)
{
     unsigned retval;
     static const int LAST_HALF_MASK = 0xF0;
     
     assert(buf);
     retval = (buf[1] & LAST_HALF_MASK) >> 4;
     retval += buf[2] << 4;
     return retval;
}

/*
 * Reads in the 16bit File allocation table into a buffer then
 * puts the values into an array in a byte order independant 
 * fashion.
 */
static clust_t *
read_fat16(unsigned long *entries, off_t offset, int buf_size)
{
     int i, fat_index = 0;
     u_int8_t *buffer;
     clust_t *fat_array;
     /* Reserved clusters are never used, and the last cluster
      * marker is always 0xffff, but Scott Mueller's book 
      * says that it could be anywhere from 0xfff8 to 0xffff.
      * because of this I have added the confusing mess of MIN
      * and MAX values for the conditions.
      */
     const unsigned int RESERVED_MIN = 0xFFF0;
     const unsigned int RESERVED_MAX = 0xFFF6;
     const unsigned int BAD16 = 0xFFF7;
     const unsigned int LAST_MIN = 0xFFF8;
     const unsigned int LAST_MAX = 0xFFFF;
    
     assert(entries);
     if (!buf_size)
          return NULL;

     *entries = buf_size / 2;
     fat_array = emalloc(*entries * sizeof *fat_array);
     buffer = emalloc(buf_size);
     if (!read_data(buffer, offset, buf_size))
          return NULL;

     for (i = 0; i < buf_size; i += 2, fat_index++) {
          fat_array[fat_index].fat_entry = little_endian_16(buffer + i);
          if ((RESERVED_MIN <= fat_array[fat_index].fat_entry) &&  
              (fat_array[fat_index].fat_entry <= RESERVED_MAX))
               fat_array[fat_index].fat_entry = RESERVED;
          else if ((LAST_MIN <= fat_array[fat_index].fat_entry) &&
                   (fat_array[fat_index].fat_entry <= LAST_MAX))
               fat_array[fat_index].fat_entry = LAST;
          else if (fat_array[fat_index].fat_entry == BAD16)
               fat_array[fat_index].fat_entry = BAD;           
     }  
     
     free(buffer);
     return fat_array;
}  

/*
 * Reads in the 32bit File allocation table into a buffer then
 * puts the values into an array in a byte order independant 
 * fashion.
 */
static clust_t *
read_fat32(unsigned long *entries, off_t offset, int buf_size)
{
     int i, fat_index = 0;
     u_int8_t *buffer;
     clust_t *fat_array;
     /* See the note in read_fat16() for explaination of these const's
      */
     const unsigned long RESERVED_MIN = 0xFFFFFF0;
     const unsigned long RESERVED_MAX = 0xFFFFFF6;
     const unsigned long BAD32 = 0xFFFFFF7;
     const unsigned long LAST_MIN = 0xFFFFFF8;
     const unsigned long LAST_MAX = 0xFFFFFFF;
     
     assert(entries);
     if (!buf_size)
          return NULL;

     *entries = buf_size / 4;
     fat_array = emalloc(*entries * sizeof *fat_array);
     buffer = emalloc(buf_size);
     if (!read_data(buffer, offset, buf_size))
          return NULL;

     for (i = 0; i < buf_size; i += 4, fat_index++) {
          fat_array[fat_index].fat_entry = little_endian_32(buffer + i);
          if ((RESERVED_MIN <= fat_array[fat_index].fat_entry) &&  
              (fat_array[fat_index].fat_entry <= RESERVED_MAX))
               fat_array[fat_index].fat_entry = RESERVED;
          else if ((LAST_MIN <= fat_array[fat_index].fat_entry) &&
                   (fat_array[fat_index].fat_entry <= LAST_MAX))
               fat_array[fat_index].fat_entry = LAST;
          else if (fat_array[fat_index].fat_entry == BAD32)
               fat_array[fat_index].fat_entry = BAD;           
     }
     free(buffer);
     return fat_array;
}  
