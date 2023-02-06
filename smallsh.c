#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> /* For pid_t type */
#include <sys/wait.h>
#include <errno.h>
#include <err.h>

// Main function takes no arguments:
// User will supply input in response to the command prompt
int main(void) {
  // Explicitly set errno prior to doing anything
  errno = 0;
/* TODO: Print an interactive input prompt */
  // Infinite loop to perform the shell functionality
  for (;;) {
    // Check for any unwaited-for background processes in the same process group ID as this program
    // Get smallsh's PGID
    // pid_t curr_pgid;
    // curr_pgid = getpgrp();
    // Error check getpgrp not necessary, always successfully returns PGID
    // if (errno != 0) err(errno, "Unable to get group ID for current process");
    // Get all processes with curr_pgid as PGID and print correct message for each
    // Pass 0 as PID to waitpid to request status for any child process with same process group ID as smallsh
    int *child_proc_status = NULL;
    pid_t child_proc_pid = waitpid(0, child_proc_status, WNOHANG);
    // Error check waitpid
    if (child_proc_pid == -1) err(errno, "Unable to retrieve pid");
    /* GO TO EXIT FOR DEBUGGING PURPOSES! REMOVE AFTER TESTING! */
    goto exit;
    // Print interactive input prompt for user
  }

/* TODO: Parse command line input into semantic tokens */
/* TODO: Implement parameter expansion for shell special parameters $$, $?, and $! and tilde expansion */
/* TODO: Implement two shell built-in commands: exit and cd */
/* TODO: Execute non-built-in commands using the appropriate exec(3) function 
 *    implement redirection operators < and >
 *    implement & operator to run commands in background */
/* TODO: Implement custom behavior for SIGINT and SIGTSTP signals */

  // Exit label for successful exiting of main function/program
exit:
  return 0;
}
