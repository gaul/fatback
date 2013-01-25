/*******
 * $Id: cmd_lostchains.c,v 1.2 2001/05/30 15:41:23 harbourn Exp $
 * lostchains command for fatback
 * displays a list of lost cluster chains
 *******/

#include <stdio.h>
#include "fatback.h"
#include "interface.h"
#include "interface_data.h"
#include "fat.h"
#include "vbr.h"
#include "output.h"

/*
 * Display a list of the lost cluster chains
 */
void cmd_lostchains(int argc, char *argv[])
{
     unsigned long i, fatsize, num_clusts;
     int found = 0;

     /* determine what the last cluster will be */
     fatsize = vbr->bytes_per_sect;
     fatsize *= vbr->sects_per_fat;
     switch (get_fs_type(vbr)) {
     case VBR_FAT12:
          num_clusts = (fatsize * 2) / 3;
          break;
     case VBR_FAT16:
          num_clusts = fatsize / 2;
          break;
     case VBR_FAT32:
          num_clusts = fatsize / 4;
          break;
     default:
          return;
     }
  
     display(NORMAL, "Lost cluster chains found starting at the following clusters:\n");
     for (i = 2; i < num_clusts; i++)
          if (clusts[i].fat_entry && 
              !clusts[i].owner && 
              (clusts[i].flags & CLUST_LOST) &&
              !clust_is_resvd(&clusts[i]) &&
              !clust_is_bad(&clusts[i])) {
               flag_chain(clusts, i, CLUST_LOST);
               display(NORMAL, "%lu\t", i);
               found = 1;
          }
     if (!found)
          display(NORMAL, "No lost cluster chains found.\n");
}
