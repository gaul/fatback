/* $Id: recovery.h,v 1.2 2001/05/30 15:47:04 harbourn Exp $
 * Output handeling module for fatback
 */
#ifndef RECOVERY_H
#define RECOVERY_H

#include "fat.h"
#include "dirtree.h"

extern int extract_file(dirent_t *, clust_t *, unsigned long, unsigned long, unsigned long, unsigned long, char *);
extern char *unused_fname(char *);
extern int stat_is_dir(char *);
extern char *fn_cat(char *, char *);
extern int make_dir(char *);
extern char *replace_tilde(char *);

#endif /*OUTPUT_H*/
