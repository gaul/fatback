/* mbr.h - header for MBR processing module of fatback */

#ifndef MBR_H
#define MBR_H

#include <sys/types.h>

struct part_range_s {   /* Holds byte offsets, not sector numbers */
     off_t start;
     off_t end;
};

extern int map_partitions(void);
extern struct part_range_s *get_prange(int);
/* The numbering convention of partitions will be for entries in the 
 * MBR to get counted first, (the entry pointing to the
 * next extended partition is not counted.)  The next number will be the
 * first extended partition, and so on. For example, if you had an MBR
 * with 2 primary partitions and 1 extended partition, the primary 
 * partitions would be 0 and 1 respectively, and the extended partition
 * would be 2.
 */

#endif











