/* $Id: output.h,v 1.5 2001/05/30 15:47:04 harbourn Exp $
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdio.h>

typedef enum {
	 NORMAL = 1,
	 VERBOSE = 2,
     LOGONLY = 3
} displaylevel_t;

int audit_init(char *, char **);
int display(displaylevel_t, char *, ...);
void ticmarker(void);
void audit_close(void);
void set_ostream(FILE *);
void reset_ostream(void);

#endif /*OUTPUT_H*/
