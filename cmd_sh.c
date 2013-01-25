/*****
 * $Id: cmd_sh.c,v 1.2 2001/01/25 09:08:51 harbourn Exp $
 * sh command for fatback
 *****/

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "interface_data.h"
#include "interface.h"

/* 
 * Execute a command in the outside environment
 * similar to the system() function except with
 * proper signal handeling.
 */
void cmd_sh(int argc, char *argv[])
{
     pid_t pid;
     int status;
     struct sigaction ignore, saveintr, savequit;
     sigset_t chldmask, savemask;

     if (argc < 2) {
	  printf("Usage: sh [command]\n");
	  return;
     }
     /* move argc and argv past the initial "sh" */
     argc--;
     argv++;
     
     /* This is basically and intellegent implementation of 
      * system() with signal handling */
     ignore.sa_handler = SIG_IGN;
     sigemptyset(&ignore.sa_mask);
     ignore.sa_flags = 0;
     if ((sigaction(SIGINT, &ignore, &saveintr) < 0) ||
	 (sigaction(SIGQUIT, &ignore, &saveintr))) {
	  printf("Error assigning sigaction\n");
	  return;
     }
     sigemptyset(&chldmask);
     sigaddset(&chldmask, SIGCHLD);
     if (sigprocmask(SIG_BLOCK, &chldmask, &savemask) < 0) {
	  printf("Error setting sigprocmask\n");
	  return;
     }
     if ((pid = fork()) == 0) {
	  /* child */
	  sigaction(SIGINT, &saveintr, NULL);
	  sigaction(SIGQUIT, &savequit, NULL);
	  sigprocmask(SIG_SETMASK, &savemask, NULL);
	  execvp(argv[0], &argv[0]);
	  exit(127); /* if an error occurred */
     } else {
	  /* parent */
	  while (waitpid(pid, &status, 0) < 0)
	       if (errno != EINTR)
		    break;
     }
     /* now restore the previous actions and masks */
     if ((sigaction(SIGINT, &saveintr, NULL) < 0) ||
	 (sigaction(SIGQUIT, &savequit, NULL) < 0) ||
	 (sigprocmask(SIG_SETMASK, &savemask, NULL) < 0))
	  printf("Warning: signal handling not fully restored after returning from process\n");
     return;
}



