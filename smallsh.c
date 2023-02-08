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
  // Set some variables we will use inside the main loop
  /*char const *restrict env_name_1 = "USER";
  char const *restrict env_name_2 = "LOGNAME";*/
  
  char *line = NULL;
  size_t n = 0;
  
  int child_proc_status;
  pid_t child_proc_pid;

  char const *restrict ifs_var_name = "IFS";
  
  char **word_tokens = NULL;
  size_t num_tokens = 0;
  // Explicitly set errno prior to doing anything
  errno = 0;

  // Infinite loop to perform the shell functionality
  for (size_t i = 0; i < 5; ++i) {
    // 1a. Manage background processes
    // Check for any unwaited-for background processes in the same process group ID as this program
    // Loop through all child processes that have the same process group ID
    // Passing 0 to waitpid as the pid means waitpid will check all children with the current process's process group ID
    // Print correct info message to stderr for all child processes found
    while ((child_proc_pid = waitpid(0, &child_proc_status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
      // Use of macros comes from CS344 modules and Linux Programming Interface text
      if (WIFEXITED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_proc_pid, WEXITSTATUS(child_proc_status)) < 0) goto exit;
      } else if (WIFSIGNALED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_proc_pid, WTERMSIG(child_proc_status)) < 0) goto exit;
      } else if (WIFSTOPPED(child_proc_status)) {
        if (kill(child_proc_pid, SIGCONT) == -1) err(errno, "Unable to send signal");
        if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_proc_pid) < 0) goto exit;
      }
    }

    // Checking errno taken from Linux Programming Interface wait example, chap. 26
    if (errno != ECHILD && errno != 0) goto exit;
    // Reset errno 
    errno = 0;
    
    //1b. PS1 expansion/prompt display
    // Determine if user or root and print prompt for user
    /*if (strcmp(getenv(env_name_1), "root") == 0 || strcmp(getenv(env_name_2), "root") == 0) {
      // prompt = '#';
      if (fprintf(stderr, "#") < 0) goto exit;
    } else {
      if (fprintf(stderr, "$") < 0 ) goto exit;
    }*/
    char *prompt = NULL;
    prompt = getenv("PS1");
    if (!prompt) prompt = "";
    if (fprintf(stderr, "%s", prompt) < 0) goto exit;

    
    //1c. Reading a line of input
    // Read a line of input from stdin
    // Code taken from example in smallsh assignment specs
    ssize_t line_length = getline(&line, &n, stdin);
    if (line_length == -1 && errno != EOF) goto exit;
    if (fprintf(stderr, "Line entered was: %s", line) < 0) goto exit;
    
    // Clear EOF and error indicators for stdin
    if (feof(stdin) != 0 || ferror(stdin) != 0) {
      // Per man pages this function should not fail
      if (fprintf(stderr, "Clearing EOF and errors") < 0) goto exit; /* DEBUGGING */
      clearerr(stdin);
    }
    // Reset errno in case EOF was encountered when reading from stdin
    errno = 0;

    // SIGNAL HANDLING IMPLEMENTED HERE
    
    //2. Word splitting
    // Get IFS environment variable value and set delimiter for use in strtok
    char *word_delim = getenv(ifs_var_name);
    // Only change value pointed to by word_delim if it is NULL, otherwise may end up changing the env variable
    if (!word_delim) {
      word_delim = " \t\n";
    }

    // Split line into words on the given delimiters
    // Always split at least one time
    char *token = strtok(line, word_delim);
    // size_t token_len = strlen(token);
    // size_t num_tokens = 1; /* To be used for tracking number of pointers in word_tokens */
    if (token != NULL) { 
      ++num_tokens;
      word_tokens = malloc(sizeof token * num_tokens); /* allocate space in tokens */
      if (!word_tokens) goto exit; /* Error checks malloc */
    
      word_tokens = &token;
    }

    for (;;) {
      token = strtok(NULL, word_delim);
      if (!token) break;
      ++num_tokens;
      word_tokens = realloc(word_tokens, num_tokens); /* Make sure there is enough space in word_tokens for new pointer */
      if (!word_tokens) goto exit; /* Error checks realloc? Will this lead to losing access to word_tokens if realloc fails? */
      char *dup_token = strdup(token); /* Duplicate the token so */
      if (!dup_token) goto exit; /* Error checks strdup */
      *word_tokens = dup_token; /* Add pointer to new_token to word_tokens */
      free(dup_token);
    }
    
    if (num_tokens > 0) {
      for (size_t i = 0; i < num_tokens; ++i) {
        if (fprintf(stderr, "Token %zd is %s\n", i, word_tokens[i]) < 0) goto exit;
      }
   
      // Free and reset word_tokens and reset num_tokens
      for (size_t j = 0; j < num_tokens; ++j) {
        free(word_tokens[j]);
      }
    }
    free(word_tokens);
    num_tokens = 0;
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
  // Free line pointer used for input
  free(line);
  for (size_t x = 0; x < num_tokens; ++x) {
    free(word_tokens[x]);
  }
  free(word_tokens);
  return errno ? -1 : 0;
}


