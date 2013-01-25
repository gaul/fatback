/************
 * $Id: interface_data.h,v 1.1 2001/01/10 13:56:03 harbourn Exp $
 * Interface data module for fatback 
 ************/

#ifndef INTERFACE_DATA_H
#define INTERFACE_DATA_H

#include "interface.h"
#include "fat.h"
#include "dirtree.h"
#include "vbr.h"

extern int stop_code;
extern clust_t *clusts;
extern dirent_t *cwd;
extern vbr_t vbr;
extern dirent_t *root_dir;
extern const char delim;
extern char *prompt;
extern command_t commands[];

void show_commands_ptr(void);

#endif /*INTERFACE_DATA_H*/





