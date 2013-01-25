/* fatback.h - main header for fatback program. contains global constants */
#ifndef FATBACK_H
#define FATBACK_H

#define SECTOR_SIZE 512

typedef enum { MBR_BLANK    = 0x00,
	       MBR_FAT16_S  = 0x04,
	       MBR_FAT_EXT  = 0x05,
	       MBR_FAT16_L  = 0x06,
	       MBR_FAT32    = 0x0B,
	       MBR_FAT32X   = 0x0C,
	       MBR_FAT16X   = 0x0E,
	       MBR_FAT_EXTX = 0x0F,
	       VBR_FAT12    = 1,
	       VBR_FAT16    = 2,
	       VBR_FAT32    = 3,
	       INVALID      = 0x00
} fs_id_t;

extern int undel_partition(int, int);

#endif /* FATBACK_H */



