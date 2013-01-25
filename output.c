/* $Id: output.c,v 1.8 2001/05/30 15:47:04 harbourn Exp $
 * Screen and Log output handeling module for fatback
 * (c)2000-2001 DoD Computer Forensics Lab
 * By SrA Nicholas Harbour
 */

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <errno.h>
#include "output.h"
#include "vars.h"

static FILE *Orig_ostream;
static FILE *ostream;
static FILE *Audit_log;
static void log_env(char **);

/*
 * Initialize audit logging
 */
int audit_init(char *log_name, char *argv[])
{
     Orig_ostream = stdout;
     ostream = stdout;
     if (!log_name) {
          log_name = "./fatback.log";
          printf("No audit log specified, using \"%s\"\n", log_name);
     } 
     if (!(Audit_log = fopen(log_name, "w"))) {
          perror("error opening audit log");
          return -1;
     }
     setbuf(Audit_log, NULL);  /* Disable stream buffering */
     log_env(argv);
     return 0;
}

/*
 * Display arguments to the screen/audit log
 */
int display(displaylevel_t level, char *format, ...)
{
     va_list arg_list;
     fbvar_t *verbose_var;
     unsigned verbose;
     int retval;
     
     /* get the verbosity level from the fatback symbol table */
     if (!(verbose_var = get_fbvar("verbose"))) {
          printf("Error reading variable\n");
          return -1;
     } else {
          verbose = verbose_var->val.ival;
          free(verbose_var);
     }

     /* print the rest of the arguments in standard printf style */
     va_start(arg_list, format);
     retval = vfprintf(Audit_log, format, arg_list);
     va_end(arg_list);
     if ((level < VERBOSE) || (verbose && level == VERBOSE)) {
          va_start(arg_list, format);
          vfprintf(ostream, format, arg_list);
          va_end(arg_list);
     }

     return retval;
}

/*
 * keep track of and display a |/-\|/-\ sequence
 */
void ticmarker(void)
{
     char *tics = "|/-\\|/-\\";
     const int numtics = 8;
     static int currtic;

     printf("\r%c", tics[currtic]);
     currtic = (currtic + 1) % numtics;
}

/*
 * Log the current environment
 */
static void log_env(char *argv[])
{
     time_t start_time;
     char hostname[MAXHOSTNAMELEN];
     char cwd[PATH_MAX];
#ifdef HAVE_UNAME
     struct utsname uname_info;
#endif /*HAVE_UNAME*/
     int i;
     char *version;

#ifdef VERSION
     version = VERSION;
#else 
     version = "(unknown)";
#endif /*VERSION*/

     /* grab some info about the current environment */
#ifdef HAVE_GETHOSTNAME
     gethostname(hostname, MAXHOSTNAMELEN);
#endif /*HAVE_GETHOSTNAME*/
     time(&start_time);
     display(LOGONLY, "Running Fatback v%s\n", version);
     display(LOGONLY, "Command Line: ");
     for (i = 0; argv[i]; i++)
          display(LOGONLY, "%s ", argv[i]);
     display(LOGONLY, "\n");
     display(LOGONLY, "Time: %s", ctime(&start_time));
#ifdef HAVE_UNAME
     uname(&uname_info);
     display(LOGONLY, "uname: %s %s %s %s %s\n", 
             uname_info.sysname, 
             uname_info.nodename,
             uname_info.release,
             uname_info.version,
             uname_info.machine
          );
#endif /*HAVE_UNAME*/
#ifdef HAVE_GETCWD
     display(LOGONLY, "Working Dir: %s\n", getcwd(cwd, PATH_MAX));
#endif /*HAVE_GETCWD*/
}

/*
 * Close the audit log
 */
void audit_close(void)
{
     fclose(Audit_log);
}

void set_ostream(FILE *stream)
{
     ostream = stream;
}

void reset_ostream(void)
{
     ostream = Orig_ostream;
}
