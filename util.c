/* util.c - This file is a place to store all the various utility functions
 * someday in the future this stuff will probably be made into a library
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>

/*
 * Handle memory allocation in a robust way
 */
void *emalloc(size_t size)
{
     void *retval;
     if (!size)
	  return NULL;
     errno = 0;
     retval = malloc(size);
     if (!retval) {
	  perror("CRITICAL ERROR");
	  exit(-1);
     }
     if (!retval) {
	  fprintf(stderr, "Fatal Error: Unable to allocate memory\n");
	  exit(-1);
     }
     return retval;
}

/*
 * Handle memory re-allocation in a robust way
 */
void *erealloc(void *ptr, size_t size)
{
     void *retval;

     errno = 0;
     retval = realloc(ptr, size);
     if (errno) {
	  perror("CRITICAL ERROR");
	  exit(-1);
     }
     if (!retval) {
	  fprintf(stderr, "Fatal Error: Unable to allocate memory\n");
	  exit(-1);
     }
     return retval;
}

/*
 * Free memory in a robust way as well as zeroing out the
 * pointer
 */
void efree(void **ptr)
{
     errno = 0;
     free(*ptr);
     if (errno) {
	  perror("CRITICAL ERROR");
	  exit(-1);
     }
     *ptr = NULL;
}

/*
 * not as useful as little_endian_16 or 32, but 
 * provided just for consistancy
 */
unsigned little_endian_8(u_int8_t *buffer)
{
     assert(buffer);
     
     /* there is no conversion to be done with a single byte */
     return *buffer;
}

/*
 * convert 16bit values stored in little endian
 * form to host byte order and return.
 */
unsigned little_endian_16(u_int8_t *buffer)
{
     register unsigned retval;

     assert(buffer);
     retval = buffer[0];
     retval += buffer[1] << 8;
     return retval;
}

/*
 * convert 32bit values stored in little endian
 * form to host byte order and return.
 */
unsigned long little_endian_32(u_int8_t *buffer)
{
     register unsigned long retval;

     assert(buffer);
     retval = buffer[0];
     retval += buffer[1] << 8;
     retval += buffer[2] << 16;
     retval += buffer[3] << 24;
     return retval;
}

/*
 * convert 32bit values stored in big endian form
 * to host byte order and retrun
 */
unsigned long big_endian_32(u_int8_t *buffer)
{
     register unsigned long retval;
     
     assert(buffer);
     retval = buffer[0] << 24;
     retval += buffer[1] << 16;
     retval += buffer[2] << 8;
     retval += buffer[3];
     return retval;
}
