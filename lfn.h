/***********
 * $Id: lfn.h,v 1.2 2000/12/11 12:29:54 harbourn Exp $
 * Long Filename Processing Module for Fatback
 ***********
 */

#ifndef LFN_H
#define LFN_H

#include <sys/types.h>
#include "dirtree.h"

typedef struct lfn_s {
     struct lfn_s *next;
     struct dirent_s *dir;
     u_int8_t *data;
     int dir_seq_num;  /* sequence number in the directory */
     int lfn_seq_num;
     int checksum;
} lfn_t;     /* no, this is not an abbriviation for elephant */

extern lfn_t *parse_lfn(u_int8_t *);  /* builds an lfn structure 
				       * if the buffer is in fact a
				       * fragment. */
extern void cat_lfn_list(lfn_t *);
extern void cat_lfn_tree(struct dirent_s *);
extern void lfn_assoc_tree(struct dirent_s *);
extern void unichoke_tree(struct dirent_s *);

#endif  /*LFN_H*/






