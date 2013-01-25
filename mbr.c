/* $Id: mbr.c,v 1.19 2001/05/30 15:47:03 harbourn Exp $ 
 * MBR processing module for fatback 
 */

/* the goal of this module is to provide an abstract way of reading partitions.
 * one highlight is the linear partition numbering scheme (described in mbr.h)
 */
#include <stdlib.h>
#include "mbr.h"
#include "output.h"
#include "input.h"
#include "util.h"
#include "fatback.h"
#include "sig.h"
#include "vars.h"

struct ptable_list_s {
     struct ptable_list_s *next;
     struct ptable_entry *table;
     unsigned long offset;
};

struct ptable_entry {
     unsigned boot_indicator;
     unsigned start_head;
     unsigned start_cyl;
     unsigned start_sect;
     unsigned sys_indicator;
     unsigned end_head;
     unsigned end_cyl;
     unsigned end_sect;
     unsigned long offset;
     unsigned long sectors;
};

static struct ptable_list_s *Ptable_list;

static struct ptable_list_s *build_ptable(off_t);
static int read_ptable(off_t, struct ptable_entry *);
static int scheck_ptable(struct ptable_entry *, sig_t);
static int scheck_boot_indicator(struct ptable_entry *);
static int scheck_sys_indicator(struct ptable_entry *);
static int scheck_part_range(struct part_range_s *);
static int is_part(struct ptable_entry *);
static int is_extended_part(struct ptable_entry *);
static struct ptable_list_s *find_ext_pentry(off_t, struct ptable_entry *);
static int part_count(struct ptable_list_s *);

static const int NUM_PTABLE_ENTRIES = 4;

enum { NON_BOOT = 0x00,
       BOOTABLE = 0x80
};

/*
 * This is the main initialization interface to this module.
 * it reads in the partition tables recursively and sets the 
 * Sector_size variable for the module
 */
int map_partitions(void)
{
     if (!(Ptable_list = build_ptable((off_t)0))) {
          free(Ptable_list);
          display(VERBOSE, "Unable to map partitions\n");
          return 0;
     } else
          return part_count(Ptable_list);
}  

/*
 * returns a structure containing the beginning and ending
 * offsets of a partition.  Note: offsets are byte offsets
 * not sector numbers.
 */
struct part_range_s *get_prange(int part_num)
{
     int i = 0, j;
     fbvar_t *sectsize_var;
     unsigned sectsize;
     struct ptable_entry *entry = NULL;
     struct ptable_list_s *pnode, *matching_node = NULL;
     struct part_range_s *prange = emalloc(sizeof *prange);
     /* see mbr.h for a note about the partition numbering convention */
     
     /* retrieve the sector size from the fatback variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize set to 0!\n");
          free(sectsize_var);
          return NULL;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);

     /* find the matching partition for the given part_num */
     for (pnode = Ptable_list; pnode && !entry; pnode = pnode->next) {
          for (j = 0; (j < NUM_PTABLE_ENTRIES) && !entry; j++) {
               struct ptable_entry *part = &pnode->table[j];
               if (is_part(part) && !is_extended_part(part)) {
                    if (i == part_num) {
                         matching_node = pnode;
                         entry = part;
                    } else
                         i++;
               }
          }
     }

     /* if a matching partition for part_num was found, 
      * compute the appropriate values */
     if (entry) {
          prange->start = (matching_node->offset + entry->offset);
          prange->start *= sectsize;
          prange->end = prange->start;
          prange->end += (entry->sectors * sectsize);
     }
     return entry? prange : NULL;
}     

/* 
 * Find out how many partitions have been maped
 */
static int part_count(struct ptable_list_s *pnode)
{
     int i, ptotal = 0;

     if (!pnode || !pnode->table)
          return 0;
     for (i = 0; i < NUM_PTABLE_ENTRIES; i++)
          if (is_part(&pnode->table[i]) && !is_extended_part(&pnode->table[i]))
               ptotal++;

     return ptotal + part_count(pnode->next);
}

/*
 * Find the extended partition entry in a partition table
 * and build the partition if it exists.
 */
static struct ptable_list_s *
find_ext_pentry(off_t offset, struct ptable_entry *table)
{
     int i;
     fbvar_t *sectsize_var;
     unsigned sectsize;

     /* get the sector size from the fatback variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize set to 0!\n");
          free(sectsize_var);
          return 0;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);
     
     /* find the extend entry in the partition table */
     for (i = 0; i < NUM_PTABLE_ENTRIES; i++)
          if (is_extended_part(&table[i])) {
               offset += table[i].offset * sectsize;
               return (build_ptable(offset));
          }
     return NULL;
}

/* 
 * Construct a partition table from the input
 * and add it to the ptable list
 */
static struct ptable_list_s *build_ptable(off_t offset)
{
     struct ptable_list_s *ext_ptable;
     const size_t ptable_size = NUM_PTABLE_ENTRIES * sizeof *ext_ptable->table;
     fbvar_t *sectsize_var;
     unsigned sectsize;

     /* get the sector size from the fatback variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize set to 0!\n");
          free(sectsize_var);
          return 0;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);

     ext_ptable = emalloc(sizeof *ext_ptable);
     ext_ptable->table = emalloc(ptable_size);
     ext_ptable->offset = offset / sectsize;
     if (!read_ptable(offset, ext_ptable->table)) {
          free(ext_ptable->table);
          free(ext_ptable);
          return NULL;
     }
     ext_ptable->next = find_ext_pentry(offset, ext_ptable->table);
     return ext_ptable;
}
          
/*
 * Actually read the partition table elements into their
 * appropriate structure.  byte order conversion handeling included
 */
static int read_ptable(off_t offset, struct ptable_entry *table)
{
     int i;
     sig_t signature;
     u_int8_t *index, *buffer;
     fbvar_t *sectsize_var;
     unsigned sectsize;
     enum { PTABLE_OFFSET      = 446,
            BOOT_INDICATOR_OFF = 0,
            START_HEAD_OFF     = 1,
            START_SECT_OFF     = 2,
            START_CYL_OFF      = 3,
            SYS_INDICATOR_OFF  = 4,
            END_HEAD_OFF       = 5,
            END_SECT_OFF       = 6,
            END_CYL_OFF        = 7,
            SECTOR_OFFSET_OFF  = 8,
            TOTAL_SECTORS_OFF  = 12,
            CYL_MASK           = 0xC0,
            SECT_MASK          = ~CYL_MASK,
            PTABLE_SIZE        = 16
     };

     if (!table)
          return 0;

     /* get the sector size from the fatback global variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize set to 0!\n");
          free(sectsize_var);
          return 0;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);

     buffer = emalloc(sectsize);
     if (!read_data(buffer, offset, sectsize)) {
          return 0;
     }
     index = buffer + PTABLE_OFFSET;

     /* Load partition table elements into their apropriate struct.  This may
      * seem a little cumbersome, but it is the easiest way to do it PORTABLY
      */
     for (i = 0; i < NUM_PTABLE_ENTRIES; i++) {
          index += !!i * PTABLE_SIZE;
          table[i].boot_indicator = little_endian_8(index +BOOT_INDICATOR_OFF);
          table[i].start_head = little_endian_8(index + START_HEAD_OFF);
          table[i].start_cyl = little_endian_8(index + START_CYL_OFF);
          table[i].start_cyl += (index[START_SECT_OFF] & CYL_MASK) << 2;
          table[i].start_sect = index[START_SECT_OFF] & SECT_MASK;
          table[i].sys_indicator = little_endian_8(index + SYS_INDICATOR_OFF);
          table[i].end_head = little_endian_8(index + END_HEAD_OFF);
          table[i].end_cyl = little_endian_8(index + END_CYL_OFF);
          table[i].end_cyl += (index[END_SECT_OFF] & CYL_MASK) << 2;
          table[i].end_sect = index[END_SECT_OFF] & SECT_MASK;
          table[i].offset = little_endian_32(index + SECTOR_OFFSET_OFF);
          table[i].sectors = little_endian_32(index + TOTAL_SECTORS_OFF);
     }

     signature = read_sig(&buffer[sectsize - 2]);
     free(buffer);
     return scheck_ptable(table, signature);
}

/*
 * tell if a partition table entry is an extended partition
 */
static int is_extended_part(struct ptable_entry *entry)
{
     /* This assumes that the partition is valid */
     if (entry->sys_indicator == MBR_FAT_EXT ||
         entry->sys_indicator == MBR_FAT_EXTX)
          return 1;
     else
          return 0;
}

/* 
 * Sanity check a partition table
 */
static int scheck_ptable(struct ptable_entry *table, sig_t sig)
{
     int i, num_partitions = 0;
     struct part_range_s prange[NUM_PTABLE_ENTRIES];
     fbvar_t *sectsize_var;
     unsigned sectsize;

     /* get the sector size from the fatback variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize set to 0!\n");
          free(sectsize_var);
          return 0;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);

     /* make sure it has the proper signature bytes */
/*     if (!scheck_sig(sig)) {
 *         return 0;
 *    }
 */
     /* make sure all the entries in the table are valid */
     for (i = 0; i < NUM_PTABLE_ENTRIES; i++) {
          if (!is_part(&table[i])) {
               prange[i].start = 0;
               prange[i].end = 0;
               continue;
          }
          num_partitions++;
          /* make note of the boundries of the partition */
          prange[i].start = table[i].offset * sectsize;
          prange[i].end = prange[i].start + (table[i].sectors * sectsize);
     }
     /* check to see if the partitions overlap */
     if (num_partitions && scheck_part_range(prange))
          return 1;
     else
          return 0;
}

/*
 * Sanity check the boot indicator byte
 */
static int scheck_boot_indicator(struct ptable_entry *entry)
{
     if (entry->boot_indicator == BOOTABLE ||
         entry->boot_indicator == NON_BOOT)
          return 1;
     else 
          return 0;    
}

/* 
 * Sanity check the system indicator byte
 */
static int scheck_sys_indicator(struct ptable_entry *entry)
{
     switch (entry->sys_indicator) {
     case MBR_FAT16_S:
     case MBR_FAT_EXT:
     case MBR_FAT16_L:
     case MBR_FAT32:
     case MBR_FAT32X:
     case MBR_FAT16X:
     case MBR_FAT_EXTX:
          return 1;
          break;
     default:
          return 0;
     }
}

/* 
 * Sanity check the partition ranges of all the partitions
 * in a givin partition table.  (make sure they dont overlap)
 */
static int scheck_part_range(struct part_range_s *part_range)
{
     int i;
     
     for (i = 0; i < NUM_PTABLE_ENTRIES; i++) {
          int j;
          if (!part_range[i].start || !part_range[i].end) {
               /* break the loop cleanly, without goto */
               i = NUM_PTABLE_ENTRIES;
               continue;
          }
          for (j = i + 1; j < NUM_PTABLE_ENTRIES; j++) {
               if (!part_range[j].start || !part_range[j].end)
                    continue;
               if ((part_range[i].start >= part_range[j].start &&
                    part_range[i].start >= part_range[j].end) 
                   ||
                   (part_range[i].end >= part_range[j].start &&
                    part_range[i].end >= part_range[j].end)) {
                    display(VERBOSE, "Overlapping partitions detected\n");
                    return 0;
               }
          }
     }
     return 1;
} 

/*
 * tell if givin partition table entry is actually a partition
 */
static int is_part(struct ptable_entry *entry)
{
     if ((scheck_boot_indicator(entry)) &&
         (scheck_sys_indicator(entry))  &&
         (entry->offset && entry->sectors))
          return 1;
     else
          return 0;
}     
