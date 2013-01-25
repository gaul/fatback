/* input.h - header file for input handeling module (input.c) for fatback */

#ifndef INPUT_H
#define INPUT_H
#include <stddef.h>
#include <sys/types.h>

#ifdef HAVE_MMAP
#ifndef MAP_FILE    /* This is required for this to compile under some */
#define MAP_FILE 0  /* of the later 4.3BSD systems */
#endif /*MAP_FILE*/
#endif /*HAVE_MMAP*/


typedef enum {
     DFILE = 0,    /* A standard stream */
     RFILE = 1,    /* Descriptor Based */
#ifdef HAVE_MMAP
     MMAP = 2,     /* An MMAP object, accesed via pointer interface */
     INPUT_T_MAX = MMAP,
#else
     INPUT_T_MAX = RFILE,
#endif /*HAVE_MMAP*/
     INPUT_T_MIN = DFILE
} input_t;

/* external interfaces */
extern int input_init(input_t, ...);
extern size_t read_data(caddr_t, off_t, size_t);
extern void input_close(void);

#endif



