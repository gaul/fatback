/* $Id: fat.h,v 1.8 2001/02/08 15:58:34 harbourn Exp $
 * header file for FAT processing module for fatback
 */

#ifndef FAT_H
#define FAT_H
#include <sys/types.h>
#include "vbr.h"

typedef struct clust_s {
     unsigned long fat_entry; 
     off_t loc;   /* the location of the cluster in the input stream */
     u_int8_t flags;
     void *owner;
} clust_t;

extern clust_t *build_fat(unsigned long *, off_t, vbr_t);
extern int clust_is_resvd(clust_t *);
extern int clust_is_bad(clust_t *);
extern int clust_is_end(clust_t *);
extern void flag_chain(clust_t *, unsigned long, u_int8_t);
extern unsigned long chain_length(clust_t *, unsigned long);

enum { /* Flags for cluster array */
     CLUST_FREE      = 0x00,
     CLUST_ACTIVE    = 0x01,
     CLUST_DELETED   = 0x02,
     CLUST_RECOVERED = 0x04,
     CLUST_LOST      = 0x08
};

#endif /* FAT12_H */





