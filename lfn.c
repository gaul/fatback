/*************************
 * $Id: lfn.c,v 1.5 2001/02/08 16:01:19 harbourn Exp $
 * Long Filename Processing module for Fatback
 *************************
 */

#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include "lfn.h"
#include "dirtree.h"
#include "util.h"

static u_int8_t *unicat(u_int8_t *, int, u_int8_t *, int);
static int unistrlen(u_int8_t *, int);
static int unicopy(u_int8_t *, u_int8_t *, int);
static void lfn_assoc(dirent_t *, lfn_t *);
static char *unichoke(u_int8_t *);
static int is_legal_lfnchar(char);

/*
 * Build a long file name fragment.
 * returns the number of char's in the fragment
 * and -1 if the buffer want a valid lfn.
 */
lfn_t *parse_lfn(u_int8_t *buf)
{
     enum { SEQ_NUM_OFF = 0,
	    SEQ_NUM_LEN = 1,
	    PART_1_OFF  = 1,
	    PART_1_LEN  = 10,
	    ATTR_OFF    = 11,
	    ATTR_LEN    = 1,
	    RESVD_OFF   = 12,
	    RESVD_LEN   = 1,
	    CHK_SUM_OFF = 13,
	    CHK_SUM_LEN = 1,
	    PART_2_OFF  = 14,
	    PART_2_LEN  = 12,
	    CLUST_OFF   = 26,
	    CLUST_LEN   = 2,
	    PART_3_OFF  = 28,
	    PART_3_LEN  = 4,
	    TOTAL_LEN   = PART_3_OFF + PART_3_LEN
     };
     int i = 0;
     lfn_t *frag;

     assert(buf);
     
     /****
      * Do some preliminary sanity checking
      * before we load the data into a 
      * structure.
      ****/
     /* Make sure that the entry isnt totaly blank! */
     while (buf[i] == 0 && i < TOTAL_LEN)
	  i++;
     if (i == TOTAL_LEN || buf[RESVD_OFF] != 0)
	  return NULL;  /* The entry was blank or instantly invalid */
     /* Check the attributes, they should be set to RHSV */
/*     if (buf[ATTR_OFF] != (ATTR_RO | ATTR_HIDDEN |
 *			   ATTR_SYSTEM | ATTR_VOLUME))
 *	  return NULL;
 */

     if ((buf[CLUST_OFF] | buf[CLUST_OFF+1]) != 0)
	  return NULL;
     
     /* Now we start moving data into a structure */
     frag = emalloc(sizeof *frag);
     frag->next = NULL;
     frag->dir = NULL;
     frag->dir_seq_num = 0;
     frag->lfn_seq_num = buf[SEQ_NUM_OFF];
     frag->checksum = buf[CHK_SUM_OFF];
     /* now concatenate all of the pieces of the name and
      * store them in a single string */
     {
	  u_int8_t *temp, *temp2, *temp3, *temp4;
	  int len_temp, len_temp2, len_temp3, len_temp4;

	  temp = emalloc(PART_1_LEN+2);
	  unicopy(&buf[PART_1_OFF], temp, PART_1_LEN);
	  temp[PART_1_LEN] = '\0';
	  temp[PART_1_LEN+1] = '\0';
	  len_temp = unistrlen(temp, PART_1_LEN);
	  if (len_temp < PART_1_LEN) {
	       frag->data = temp;
	       return frag;
	  }
	  temp2 = emalloc(PART_2_LEN+2);
	  unicopy(&buf[PART_2_OFF], temp2, PART_2_LEN);
	  temp2[PART_2_LEN] = '\0';
	  temp2[PART_2_LEN+1] = '\0';
	  len_temp2 = unistrlen(temp2, PART_2_LEN);
	  if (!len_temp2) {
	       frag->data = temp;
	       free(temp2);
	       return frag;
	  }    
	  /*add the first two pieces together*/
	  temp3 = unicat(temp, len_temp, temp2, len_temp2);	  
	  len_temp3 = unistrlen(temp3, PART_1_LEN + PART_2_LEN);

	  free(temp);
	  free(temp2);

	  if (len_temp2 < PART_2_LEN) {
	       frag->data = temp3;
	       return frag;
	  }
	  /*grab the third piece */
	  temp4 = emalloc(PART_3_LEN+2);
	  unicopy(&buf[PART_3_OFF], temp4, PART_3_LEN);
	  temp4[PART_3_LEN] = '\0';
	  temp4[PART_3_LEN+1] = '\0';
	  len_temp4 = unistrlen(temp4, PART_3_LEN);
	  if (!len_temp4) {
	       frag->data = temp3;
	       free(temp4);
	       return frag;
	  }
	  /* now add on the third piece */
	  frag->data = unicat(temp3, len_temp3, temp4, len_temp4);
	  
	  free(temp3);
	  free(temp4);
     }
     return frag;
}   

/*
 * Concatenate all lfn fragments in a given list
 */
void cat_lfn_list(lfn_t *lfn_list)
{
     lfn_t *entry;
     
     assert(lfn_list);
     for (entry = lfn_list; entry && entry->next; entry = entry->next) {
	  lfn_t *next = entry->next;
	  while ((entry->next) && 
		 (entry->dir_seq_num - next->dir_seq_num == 1) && /* Reinstated */
	         (entry->checksum == next->checksum)) {
	       u_int8_t *temp = entry->data;
	       int temp_len = unistrlen(temp, 0);
	       int next_len = unistrlen(next->data, 26);
	       /* we can gain more accuracy by forcing the first fragment
		* to be 26 bytes long, which theoreticly it should be. */
	       if (temp_len < 26)
		    break;
	       entry->data = unicat(temp, temp_len, next->data, next_len);
	       entry->next = next->next;
	       free(temp);
	       free(next->data);
	       free(next);
	       next = entry->next;
	  }
     }
}    

/*
 * Recursively concatenate lfn fragments over a 
 * given directory tree
 */
void cat_lfn_tree(dirent_t *dir)
{
     assert(dir);
     if (dir->lfn_list)
	  cat_lfn_list(dir->lfn_list);
     if (dir->child)
	  cat_lfn_tree(dir->child);
     if (dir->next)
	  cat_lfn_tree(dir->next);
}
	  
/*
 * Associate all lfn's with their appropriate
 * directory entry, recursively
 */
void lfn_assoc_tree(dirent_t *dir)
{
     lfn_t *lfn;
     
     assert(dir);
     for (lfn = dir->lfn_list; lfn; lfn = lfn->next)
	  if (dir->child)
	       lfn_assoc(dir->child, lfn);
     if (dir->child)
	  lfn_assoc_tree(dir->child);
     if (dir->next)
	  lfn_assoc_tree(dir->next);
}

/*
 * Convert unicode encoded long filenames in
 * a directory tree into ascii
 */
void unichoke_tree(dirent_t *ent)
{
     char *temp;

     assert(ent);
     temp = ent->lfn;
     if (temp) {
	  ent->lfn = unichoke(ent->lfn);
	  free(temp);
     }
     if (ent->child)
	  unichoke_tree(ent->child);
     if (ent->next)
	  unichoke_tree(ent->next);
}

/*
 * Convert a unicode string into an ascii
 * filename. (without using libc)
 */
static char *unichoke(u_int8_t *lfn)
{
     char *retval;
     int retval_length, lfn_length, i;
     
     assert(lfn);
     lfn_length = unistrlen(lfn, 512); /* lfn's can't be larger
					   * than 256 letters. */
     retval_length = lfn_length / 2;  /*take half the unicode size*/
     if (!retval_length)
	  return NULL;
     retval = emalloc(retval_length+1);
     for (i = 0; i < (lfn_length/2); i++) {
	  if (!lfn[i*2] && !lfn[i*2 + 1]) {
	       retval[i] = '\0';
	       break;
	  }
	  if (is_legal_lfnchar(lfn[i*2]))
	       retval[i] = lfn[i*2];
	  else {
	       free(retval);
	       return NULL;
	  }
     }
     retval[i] = '\0';
     return retval;
}     

/*
 * Determine if a charecter is a legal 
 * long filename character.
 */
static int is_legal_lfnchar(char ch)
{
     static const char *legals = 
	  " \tABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789~!@#$%^&*()-{}'`,.";
     const char *i;
     
     for (i = legals; i; i++)
	  if (*i == ch)
	       return 1;
     return 0;
}
/*
 * Associate an lfn with the apporpriate 
 * entry in a givin list
 */
static void lfn_assoc(dirent_t *ent_list, lfn_t *lfn)
{
     dirent_t *ent;
     
     assert(ent_list && lfn);
     for (ent = ent_list; ent; ent = ent->next) {
	  if (ent->sequence_num == lfn->dir_seq_num + 1) {
	       ent->lfn = lfn->data;
	       lfn->data = NULL;
	       break;
	  }
     }
}

/*
 * Concatenate two unicode strings together
 */
static u_int8_t *unicat(u_int8_t *a, int maxa, u_int8_t *b, int maxb)
{
     int new_length;
     int i, length_a, length_b;
     u_int8_t *new_string;

     assert(a && b && maxa > 0);
     length_a = unistrlen(a, maxa);
     length_b = unistrlen(b, maxb);
     new_length = length_a + length_b;
     new_string = emalloc(new_length + 2); /* add terminator */

     /* now copy a and b into new_string */
     for (i = 0; i < length_a; i++)
	  new_string[i] = a[i];
     for (; i < new_length; i++)
	  new_string[i] = b[i-length_a];
     new_string[i++] = 0; /* add the terminator to the string */
     new_string[i] = 0; 
     
     return new_string;
}

/*
 * Determine the length in bytes of a unicode string
 * if max is specified as zero, then it is not checked.
 */
static int unistrlen(u_int8_t *str, int max)
{
     int i;
     
     assert(str);
     for (i = 0; !max || (i < max); i += 2)
	  if (str[i] == 0 && str[i+1] == 0)
	       break;
     return i;
}

/*
 * Copy Unicode characters from one buffer to another.
 * Strings are terminated with null's which are two
 * bytes wide in unicode. n is number of bytes, not unichars
 * returns the number of bytes copied.
 */
static int unicopy(u_int8_t *from, u_int8_t *to, int n)
{
     int i;

     /* this is a lot like a regular string copy except that
      * we cannot stop on a single null, we have to stop at a
      * 16 bit zero value that is evenly aligned from the 
      * beginning of the buffer 
      */

     assert(from && to);
     for (i = 0; i < n; i += 2) {
	  to[i] = from[i];
	  to[i+1] = from[i+1];
	  if (from[i] == 0 && from[i+1] == 0)
	       break;
     }
     return i;
}
     




