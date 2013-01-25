/* sig.h - Header file for MBR/VBR signature handeling module for fatback */

#ifndef SIG_H
#define SIG_H

#include <sys/types.h>
typedef u_int16_t sig_t;

extern sig_t read_sig(u_int8_t *);
extern int scheck_sig(sig_t);

#endif /* SIG_H */
