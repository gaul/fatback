/* $Id: input.c,v 1.13 2001/02/10 03:40:03 harbourn Exp $
 *Input handeling module for fatback
 * (c)2000-2001 DoD Computer Forensics Lab
 * By SrA Nicholas Harbour
 */

/* The purpose of this module is to provide a layer of abstraction for 
   the program to get input from.
*/

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "input.h"

static FILE *Input_stream; 
static int Input_fd;
static caddr_t Mem_ptr; /* for mmap() objects as input */
static struct stat File_stat; 
static int Mmap_fd;
static input_t Input_type;

/* input_ops functions for descriptor based input type */
static int dfile_init(va_list);
static size_t dfile_read(caddr_t, off_t, size_t);
static void dfile_close(void);

/* input_ops functions for RFILE input type */
static int rfile_init(va_list);
static size_t rfile_read(caddr_t, off_t, size_t);
static void rfile_close(void);

#ifdef HAVE_MMAP
/* input_ops functions for MMAP input type */
static int mmap_init(va_list);
static size_t mmap_read(caddr_t, off_t, size_t);
static void mmap_close(void);
#endif /*HAVE_MMAP*/

/*
 * Each type of input must provide three basic funcions,
 * initialize, read, and close.  this structure contains
 * pointers to those functions. 
 */
struct input_ops_s {
     int (*i_init)(va_list);
     size_t (*i_read)(caddr_t, off_t, size_t);
     void (*i_close)(void);
};

/*
 * Each supported input type must add a structure
 * to this array.  The fields are function pointers
 * as described by the input_ops_s structure.
 */
struct input_ops_s input_ops[] = {
     { /* DFILE */
	  dfile_init,
	  dfile_read,
	  dfile_close
     },
     { /* RFILE */
	  rfile_init,
	  rfile_read,
	  rfile_close
     }
#ifdef HAVE_MMAP
     ,{ /* MMAP */
	  mmap_init,
	  mmap_read,
	  mmap_close
     }
#endif /*HAVE_MMAP*/
};

/* 
 * Initialize the input based on the type.
 * This function calls the appropriate function
 * based on the input type specified.
 */  
int input_init(input_t type, ...)
{
     va_list arg_list;
 
     va_start(arg_list, type);
     Input_type = type;
     
     if ((INPUT_T_MIN <= Input_type) && (Input_type <= INPUT_T_MAX))
	  if ((*input_ops[Input_type].i_init)(arg_list)) {
	       va_end(arg_list);
	       return 0;
	  }
     va_end(arg_list);
     return -1;
}

/*
 * This is the user level interface to read
 * data from the input.  This calls the appropriate
 * function based on the input type.
 */
size_t read_data(caddr_t ptr, off_t offset, size_t size)
{
     if (!size || !ptr)
	  return 0;
     return (*input_ops[Input_type].i_read)(ptr, offset, size);
}     

/*
 * This is the user level interface to close
 * the input.  It calls the appropriate close
 * function based on the input type.
 */
void input_close(void)
{
     (*input_ops[Input_type].i_close)();
}

/*
 * Initialize function for descriptor based
 * file io.
 */
static int dfile_init(va_list arg_list)
{
     char *filename = va_arg(arg_list, char *);

     if (!filename)
	  return 0;
     errno = 0;
     stat(filename, &File_stat);
     if (errno) {
	  perror("Error in dfile_init()");
	  return 0;
     }
     Input_fd = open(filename, O_RDONLY);
     if (errno) {
	  perror("Error in dfile_init()");
	  return 0;
     }
     return 1;
}

/*
 * Read data from a file discriptor
 */
static size_t dfile_read(caddr_t ptr, off_t offset, size_t size)
{
     size_t retval;

     errno = 0;
     lseek(Input_fd, offset, SEEK_SET);
     if (errno) {
	  perror("Error in dfile_read()1");
	  return 0;
     }
     retval = read(Input_fd, ptr, size);
     if (errno) {
	  perror("Error in dfile_read()2");
	  return 0;
     }
     return retval;
}

/*
 * Close a file descriptor
 */
static void dfile_close(void)
{
     close(Input_fd);
}

/*
 * Initialize function for regular files.
 * uses streams input.
 */
static int rfile_init(va_list arg_list)
{
     char *filename = va_arg(arg_list, char *);

     if (!filename)
	  return 0;
     errno = 0;
     stat(filename, &File_stat);
     if (errno) {
	  perror("Error in rfile_init()");
	  return 0;
     }
     Input_stream = fopen(filename, "r");
     if (!Input_stream) {
	  perror("Error in rfile_init()");
	  return 0;
     } else
	  return 1;
}

/*
 * Read data from a regular file or stream.
 */
static size_t rfile_read(caddr_t ptr, off_t offset, size_t size)
{
     size_t retval;

     errno = 0;
     if (fseek(Input_stream, offset, SEEK_SET)) {
	  perror("Error in rfile_read()");
	  return 0;
     }
     else
	  if (!(retval = fread(ptr, 1, size, Input_stream))) {
	       perror("Error in rfile_read()");
	       return 0;
	  }
     return retval;
}

/*
 * Close a regular file
 */
static void rfile_close(void)
{
     fclose(Input_stream);
}

#ifdef HAVE_MMAP
/*
 * Initialize function for MMAP input
 * type.
 */
static int mmap_init(va_list arg_list)
{
     char *filename = va_arg(arg_list, char *);
     if (!filename)
	  return 0;
     errno = 0;
     stat(filename, &File_stat);
     if (errno) {
	  perror("Error in mmap_init()");
	  return 0;
     }
     if (!(Mmap_fd = open(filename, O_RDONLY)))
	  return 0;
     Mem_ptr = mmap(NULL, File_stat.st_size, PROT_READ, MAP_SHARED, Mmap_fd, 0);
     if (!Mem_ptr) {
	  perror("Error in mmap_init()");
	  return 0;
     } else
	  return 1;
}

/*
 * Read data from an MMAP input type
 */
static size_t mmap_read(caddr_t ptr, off_t offset, size_t size)
{
     if (offset + size > File_stat.st_size)
	  return 0;
     if (!memcpy(ptr, Mem_ptr + offset, size)) {
	  perror("Error in mmap_read()");
	  return 0;
     }
     return size;
}

/*
 * Close funcion for MMAP input type
 */
static void mmap_close(void)
{
     munmap(Mem_ptr, File_stat.st_size);
     close(Mmap_fd);
}

#endif /*HAVE_MMAP*/





