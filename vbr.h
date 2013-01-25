/* $Id: vbr.h,v 1.8 2001/05/30 15:47:04 harbourn Exp $
 * VBR Processing module for fatback 
 */

/* the design of this module will be for the module not to hold any static data.
 * this is important because in the future there is the possibility of these
 * functions being used in a multi-threaded way.
 */
#ifndef VBR_H
#define VBR_H

#include <sys/types.h>
#include "fatback.h"
#include "mbr.h"

struct vbr_s {
/* elements are not aligned, but listed in the order they appear in the VBR */
     char *oem_name;              /* OEM name and DOS version ("IBM 4.0") */
     unsigned bytes_per_sect;     /* Bytes per Sector */
     unsigned sects_per_clust;    /* Sectors per Cluster */
     unsigned reserved_sects;     /* Reserved Sectors */
     unsigned fat_copies;         /* Fat copies (usually 2) */
     unsigned max_rdir_entries;   /* Maximum root directory entries */
     unsigned total_sects_s;      /* Total sectors (if partition <= 32M) */
     unsigned media_descriptor;   /* Media Descriptor Byte */
     unsigned long sects_per_fat; /* Sectors per Fat */
     unsigned sects_per_track;    /* Sectors per Track */
     unsigned num_heads;          /* Number of Heads */
     unsigned long hidden_sects;  /* Hidden Sectors */
     unsigned long total_sects_l; /* Total sectors (if partition > 32M) */
     union {
          struct {
               unsigned drive_number;   /* Physical drive number */
               unsigned ext_boot_rec_sig; /* Extended boot record signature */
               char *label;                   /* Volume label */
               off_t root_dir_loc;        /* root directory location */
          } fat1216;
          struct {
               unsigned long root_dir_clust;  /* Root directory cluster */
          } fat32;
     } d;
     unsigned long serial_num;    /* Volume serial number */
     char *fs_id;                 /* File system ID */
     unsigned long fat_entries;   /* Number of entries in the FAT table */
};

typedef struct vbr_s *vbr_t;

extern vbr_t build_vbr(off_t);
extern fs_id_t get_fs_type(vbr_t);
extern unsigned long get_root_loc(off_t, vbr_t);
extern void log_vbr(vbr_t);

#endif /* VBR_H */
