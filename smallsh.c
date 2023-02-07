#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> /* For pid_t type */
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <err.h>


// Main function takes no arguments:
// User will supply input in response to the command prompt
int main(void) {
  // Set some variables we will use inside the loop
  char const *restrict env_name_1 = "USER";
  char const *restrict env_name_2 = "LOGNAME";
  char *line = NULL;
  size_t n = 0;
  int child_proc_status;
  pid_t child_proc_pid;
  // Explicitly set errno prior to doing anything
  errno = 0;

  /* TODO: Print an interactive input prompt */
  // Infinite loop to perform the shell functionality
  for (size_t i = 0; i < 5; ++i) {
    // Check for any unwaited-for background processes in the same process group ID as this program

    // Loop through all child processes that have the same process group ID
    // Passing 0 to waitpid as the pid means waitpid will check all children with the current process's process group ID
    // Print correct info message to stderr for all child processes found
    while ((child_proc_pid = waitpid(0, &child_proc_status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
      // Use of macros from CS344 modules and Linux Programming Interface text
      if (WIFEXITED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_proc_pid, WEXITSTATUS(child_proc_status)) < 0) err(errno, "Error occurred in fprintf");
      } else if (WIFSIGNALED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_proc_pid, WTERMSIG(child_proc_status)) < 0) err(errno, "Error occurred in fprintf");
      } else if (WIFSTOPPED(child_proc_status)) {
        if (kill(child_proc_pid, SIGCONT) == -1) err(errno, "Unable to send signal");
        if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_proc_pid) < 0) err(errno, "Error occurred in fprintf");
      }
    }

    // Checking errno taken from Linux Programming Interface wait example, chap. 26
    if (errno != ECHILD) err(errno, "Error occurred in waitpid");
    // Reset errno 
    errno = 0;
    
    // Print interactive input prompt for user
    // Determine if user or root
    // char const *restrict root_name = "root";
    // char prompt = '$';
    if (strcmp(getenv(env_name_1), "root") == 0 || strcmp(getenv(env_name_2), "root") == 0) {
      // prompt = '#';
      if (fprintf(stderr, "#") < 0) err(errno, "Error occurred in fprintf");
    } else {
      if (fprintf(stderr, "$") < 0 ) err(errno, "Error occurred in fprintf");
    }
    
    // Loop to get input
    // Code taken from example in smallsh assignment specs
    ssize_t line_length = getline(&line, &n, stdin);
    if (line_length == -1 && errno != EOF) err(errno, "Error occurred in getline");
    if (fprintf(stderr, "Line entered was: %s", line) < 0) err(errno, "Error occurred in fprintf");
    goto exit;
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
  free(line);
  return 0;
}
