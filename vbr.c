/* $Id: vbr.c,v 1.11 2001/05/30 15:47:04 harbourn Exp $
 * VBR processing module for fatback 
 */

#include <sys/types.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include "vbr.h"
#include "fatback.h"
#include "util.h"
#include "sig.h"
#include "output.h"
#include "input.h"
#include "vars.h"

static vbr_t read_vbr(off_t);
static int scheck_vbr(vbr_t, sig_t);

enum { OEM_NAME_LEN = 8,
       LABEL_LEN    = 11,
       FS_ID_LEN    = 8
};

/* 
 * this is the initial interface called.  it reads in a vbr and
 * sets the Sector_size variable for use by almost all the functions in
 * this module.
 */
vbr_t build_vbr(off_t offset)
{
     vbr_t vbr;

     if (!(vbr = read_vbr(offset))) {
          display(NORMAL, "Unable to read Volume Boot Record\n");
          return NULL;
     }
     return vbr;
}

/* 
 * Reads in a Volume Boot Record, and returns a pointer to it.  this
 * function calles emalloc(), and thus objects created by it need to
 * be freed. Viva Datum!
 */
static vbr_t read_vbr(off_t offset)
{
     vbr_t vbr = emalloc(sizeof *vbr);
     u_int8_t *buffer;
     sig_t signature;
     int fat32_flag = 0;
     fbvar_t *sectsize_var;
     unsigned sectsize;
     off_t serial_num_off, fs_id_off;
     int i;
     enum { OEM_NAME_OFF         = 3,
            BYTES_PER_SECT_OFF   = 11,
            SECTS_PER_CLUST_OFF  = 13,
            RESERVED_SECTS_OFF   = 14,
            FAT_COPIES_OFF       = 16,
            MAX_RDIR_ENTRIES_OFF = 17,
            TOTAL_SECTS_S_OFF    = 19,
            MEDIA_DESCRIPTOR_OFF = 21,
            SECTS_PER_FAT_OFF    = 22,
            SECTS_PER_TRACK_OFF  = 24,
            NUM_HEADS_OFF        = 26,
            HIDDEN_SECTS_OFF     = 28,
            TOTAL_SECTS_L_OFF    = 32,
            DRIVE_NUMBER_OFF     = 36,
            SECTS_PERFAT32_OFF   = 36,  /* FAT32 Specific offset */
            RDIR_CLUST_OFF       = 44,  /*       "       "       */
            SERIAL_NUM32_OFF     = 67,  /*       "       "       */
            FS_ID32_OFF          = 82,  /*       "       "       */
            EXT_BOOT_REC_SIG_OFF = 38,  /* FAT12/16 Specific offset */
            SERIAL_NUM_OFF       = 39,  /*         "        "       */
            LABEL_OFF            = 43,  /*         "        "       */
            FS_ID_OFF            = 54   /*         "        "       */
     };

     /* get the sector size from the global fatback variable table */
     sectsize_var = get_fbvar("sectsize");
     if (!sectsize_var->val.ival) {
          display(NORMAL, "Error: sectsize is set to 0!\n");
          free(sectsize_var);
          return NULL;
     }
     sectsize = sectsize_var->val.ival;
     free(sectsize_var);

     buffer = emalloc(sectsize);
     if(!read_data(buffer, offset, sectsize))
          return NULL;
     
     /* load the oem name into a string */
     vbr->oem_name = emalloc(OEM_NAME_LEN + 1);  /* add for null terminator */
     strncpy(vbr->oem_name, &buffer[OEM_NAME_OFF], OEM_NAME_LEN);
     vbr->oem_name[OEM_NAME_LEN] = '\0';         /* null terminate the string*/
     
     /* load other numeric values */
     vbr->bytes_per_sect = little_endian_16(buffer + BYTES_PER_SECT_OFF);
     vbr->sects_per_clust = little_endian_8(buffer + SECTS_PER_CLUST_OFF);
     vbr->reserved_sects = little_endian_16(buffer + RESERVED_SECTS_OFF);
     vbr->fat_copies = little_endian_8(buffer + FAT_COPIES_OFF);
     vbr->max_rdir_entries = little_endian_16(buffer + MAX_RDIR_ENTRIES_OFF);
     /* if max_rdir_entries is null, well proceed assuming this is FAT32 */
     if (!vbr->max_rdir_entries)
          fat32_flag++;
     vbr->total_sects_s = little_endian_16(buffer + TOTAL_SECTS_S_OFF);
     vbr->media_descriptor = little_endian_8(buffer + MEDIA_DESCRIPTOR_OFF);
     vbr->sects_per_fat = little_endian_16(buffer + SECTS_PER_FAT_OFF);
     vbr->sects_per_track = little_endian_16(buffer + SECTS_PER_TRACK_OFF);
     vbr->num_heads = little_endian_8(buffer + NUM_HEADS_OFF);
     vbr->hidden_sects = little_endian_32(buffer + HIDDEN_SECTS_OFF);
     vbr->total_sects_l = little_endian_32(buffer + TOTAL_SECTS_L_OFF);
     if (fat32_flag) {
          vbr->sects_per_fat = little_endian_32(buffer + SECTS_PERFAT32_OFF);
          vbr->d.fat32.root_dir_clust = little_endian_32(buffer + RDIR_CLUST_OFF);
          serial_num_off = SERIAL_NUM32_OFF;
          fs_id_off = FS_ID32_OFF; 
     } else {
          vbr->d.fat1216.drive_number = little_endian_8(buffer + DRIVE_NUMBER_OFF);
          vbr->d.fat1216.ext_boot_rec_sig = little_endian_8(buffer + EXT_BOOT_REC_SIG_OFF);
          /* determine offset of root directory */ 
          vbr->d.fat1216.root_dir_loc = vbr->reserved_sects;
          vbr->d.fat1216.root_dir_loc += vbr->fat_copies * vbr->sects_per_fat;
          vbr->d.fat1216.root_dir_loc *= vbr->bytes_per_sect;
          vbr->d.fat1216.root_dir_loc += offset;
          /* load the volume label into a string */
          /* add for null terminator */
          vbr->d.fat1216.label = emalloc(LABEL_LEN + 1); 
          strncpy(vbr->d.fat1216.label, &buffer[LABEL_OFF],
                  LABEL_LEN);	
          /* null terminate the string */
          vbr->d.fat1216.label[LABEL_LEN] = '\0';
          serial_num_off = SERIAL_NUM_OFF;
          fs_id_off = FS_ID_OFF;
     }
     vbr->serial_num = little_endian_32(buffer + serial_num_off);
     
     /* load the fs id into a string */
     /* first test to make sure that it is a valid ascii string */
     for (i = 0; i < FS_ID_LEN && isascii(buffer[fs_id_off + i]); i++)
          ;
     if (i == FS_ID_LEN) {
          vbr->fs_id = emalloc(fs_id_off + 1);     /* add for null terminator */
          strncpy(vbr->fs_id, &buffer[fs_id_off], FS_ID_LEN);
          vbr->fs_id[FS_ID_LEN] = '\0';            /* null terminate the string */
     } else
          vbr->fs_id = NULL;

     /* read in the boot sector signature */
     signature = read_sig(&buffer[sectsize - 2]);
     free(buffer);
     /* run the VBR through a quick sanity check */
     if (!scheck_vbr(vbr, signature)) {
          display(NORMAL, "No valid VBR found at offset %d\n", offset);
          return NULL;
     }
     return vbr;
}

/* 
 * Sanity check the Volume Boot Record
 */
static int scheck_vbr(vbr_t vbr, sig_t signature)
{
     unsigned i, invalid = 0;

     if (!vbr || !signature)
          return 0;
     
     /* Make sure all 8 char's of OEM name are ascii */
     for (i = 0; i < OEM_NAME_LEN; i++) 
          if (!vbr->oem_name[i] || !isascii(vbr->oem_name[i]))
               invalid++;
     if (invalid) {
          display(VERBOSE, "%d characters of the OEM name in the VBR are invalid\n", invalid);
          /* return 0; */  
     }
     
     /* Make sure sectors per cluster is a power of two, or 1 */
     {
          unsigned long x = vbr->sects_per_clust;
          if ((x != 1) && (x != 2) && (x != 4) &&
              (x != 8) && (x != 16) &&
              (x != 32) && (x != 64) &&
              (x != 128) && (x != 256)) {
               display(VERBOSE, "Sectors per cluster byte is not a power of 2\n");
               return 0;
          }
     }

     /* Make sure FAT copies are non-zero, and report if > 2 */
     if (!vbr->fat_copies) {
          display(VERBOSE, "The VBR reports zero FAT copies\n");
          return 0;
     }
     if (vbr->fat_copies > 2)
          display(VERBOSE, "The VBR reports that FAT copies is greater than 2\n");
     
     /* report if Media Descriptor byte is zero */
     if (!vbr->media_descriptor) 
          display(VERBOSE, "The Media Descriptor byte in the VBR is zero\n");

     /* make sure that reserved sectors is !0, and report if !1 */
     if (!vbr->reserved_sects) {
          display(VERBOSE, "The VBR reports 0 reserved sectors\n");
          return 0;
     }

     if (!vbr->hidden_sects)
          display(VERBOSE, "The VBR reports no hidden sectors\n");

     /* report if the volume serial number is non-null */
     if (!vbr->serial_num)
          display(VERBOSE, "The VBR contains a null serial number\n");
	  
     /* ensure that total_sects_s and total_sects_t are mutually exclusive */
     if (vbr->total_sects_s && vbr->total_sects_l) {
          display(VERBOSE, "Total sectors 16bit and 32bit values exist in VBR\n");
          return 0;
     }

     /* also ensure that at least one has a value */
     if (!vbr->total_sects_s && !vbr->total_sects_l) {
          display(VERBOSE, "No value in VBR for Total Sectors\n");
          return 0;
     }
     
     /* if we have made it here, all must be good! */
     return 1;
}

/* 
 * Find out which filesystem it is. (fat12,16, or 32?)
 */
fs_id_t get_fs_type(vbr_t vbr)
{
     unsigned long total_sects, total_clusts;
     enum { FAT12_MAX_CLUSTS = 0xFFF,
            FAT16_MAX_CLUSTS = 0xFFFF,
            FAT32_MAX_CLUSTS = 0xFFFFFFFFL
     };

     assert(vbr);
     total_sects = vbr->total_sects_s ? vbr->total_sects_s :
          vbr->total_sects_l;
     total_clusts = total_sects / vbr->sects_per_clust;
     if (total_clusts <= FAT12_MAX_CLUSTS)
          return VBR_FAT12;
     if (total_clusts <= FAT16_MAX_CLUSTS)
          return VBR_FAT16;
     return VBR_FAT32;
}

/* 
 * Determine what cluster the the root directory begins.
 */
unsigned long get_root_loc(off_t offset, vbr_t vbr)
{
     assert(vbr);
     if (vbr->max_rdir_entries)
          return 0;
     else
          return vbr->d.fat32.root_dir_clust;
}

/* 
 * Log the Volume Boot Record information to the display log
 */
void log_vbr(vbr_t vbr)
{
     assert(vbr);
     display(VERBOSE, "oem_name: %s\n", vbr->oem_name);
     display(VERBOSE, "bytes_per_sect: %u\n", vbr->bytes_per_sect);
     display(VERBOSE, "reserved_sects: %u\n", vbr->reserved_sects);
     display(VERBOSE, "fat_copies: %u\n", vbr->fat_copies);
     display(VERBOSE, "max_rdir_entries: %u\n", vbr->max_rdir_entries);
     display(VERBOSE, "total_sects_s: %u\n", vbr->total_sects_s);
     display(VERBOSE, "media_descriptor: %x\n", vbr->media_descriptor);
     display(VERBOSE, "sects_per_fat: %u\n", vbr->sects_per_fat);
     display(VERBOSE, "sects_per_track: %u\n", vbr->sects_per_track);
     display(VERBOSE, "num_heads: %u\n", vbr->num_heads);
     display(VERBOSE, "hidden_sects: %lu\n", vbr->hidden_sects);
     display(VERBOSE, "total_sects_l: %lu\n", vbr->total_sects_l);
     display(VERBOSE, "serial_num: %lx\n", vbr->serial_num);
     display(VERBOSE, "fs_id: %s\n", vbr->fs_id ? vbr->fs_id : "(invalid)");
     display(VERBOSE, "Filesystem type is ");
     switch (get_fs_type(vbr)) {
     case VBR_FAT12:
          display(VERBOSE, "FAT12\n");
          break;
     case VBR_FAT16:
          display(VERBOSE, "FAT16\n");
          break;
     case VBR_FAT32:
          display(VERBOSE, "FAT32\n");
          break;
     default:
          display(VERBOSE, "(invalid)\n");
          break;
     }

}
