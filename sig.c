/* sig.c - Boot record signature processing module for fatback */

#include <assert.h>
#include "sig.h"
#include "input.h"

sig_t read_sig(u_int8_t *buf)
{
     sig_t retval;

     assert(buf);

     retval = buf[0];
     retval += buf[1] << 8;
     return 1;
}

int scheck_sig(sig_t sig)
{
     static const int MBR_SIGNATURE = 0xAA55;

     if (sig == MBR_SIGNATURE)
          return 1;
     else
	  return 0;
}
