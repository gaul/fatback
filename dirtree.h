/*****************
 * $Id: dirtree.h,v 1.5 2001/01/25 09:05:35 harbourn Exp $
 * Header file for the directory and directory entry
 * processing module
 *****************
 */

#ifndef DIRTREE_H
#define DIRTREE_H

#include <sys/types.h>
#include <time.h>
#include "lfn.h"
#include "fat.h"

typedef struct dirent_s {
     struct dirent_s *parent;
     struct dirent_s *next;
     struct dirent_s *child;
     u_int8_t flags;
     char *filename;
     char *lfn;
     unsigned long cluster;
     unsigned long size;
     u_int8_t attrs;
     struct tm time;
     unsigned int sequence_num;
     struct lfn_s *lfn_list; /*for use by directories*/
} dirent_t;

typedef struct entlist_s {
     struct entlist_s *next;
     dirent_t *ent;
} entlist_t;

enum { /* File attribute masks */
     ATTR_RO      = 0x01,
     ATTR_HIDDEN  = 0x02,
     ATTR_SYSTEM  = 0x04,
     ATTR_VOLUME  = 0x08,
     ATTR_DIR     = 0x10,
     ATTR_ARCHIVE = 0x20,
     ATTR_RESERVE = 0xC0
};

enum { /* Flags for entries */
     ENT_DELETED = 0x01
};

extern dirent_t *build_dir(clust_t *, unsigned long, vbr_t, dirent_t *);
extern dirent_t *build_tree(clust_t *, unsigned long, unsigned long, vbr_t, dirent_t *);
extern int is_newer(dirent_t *, dirent_t *);

#endif /* DIRTREE_H */


