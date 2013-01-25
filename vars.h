/************
 * $Id: vars.h,v 1.2 2001/01/25 08:22:24 harbourn Exp $
 * Runtime variable handeling module for fatback
 ************/

#ifndef VARS_H
#define VARS_H

typedef enum {
     FB_BOOL = 1,
     FB_INT  = 2,
     FB_STRING = 3
} fbvar_type_t;

typedef union {
     int bval;           /* FB_BOOL */
     unsigned int ival;  /* FB_INT */
     char *sval;         /* FB_STRING */
} fbvar_val_t;

typedef struct {
     char *name;
     fbvar_type_t type;
     fbvar_val_t val;
} fbvar_t;

extern int set_fbvar(char *, ...);
extern fbvar_t *get_fbvar(char *);

#endif  /*VARS_H*/
