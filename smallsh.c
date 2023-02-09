#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> /* For pid_t type */
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <err.h>

// String search function
// Adapted from instructor's string search function video linked in CS344's smallsh assignment specs
// Parameters: string to be searched, pattern to search for, substitution for found pattern
char *str_gsub(char *restrict *restrict orig_str, char const *restrict pattern, char const *restrict sub) {
  char *expanded_str = *orig_str; 
  size_t orig_str_len = strlen(expanded_str);
  size_t const pattern_len = strlen(pattern), sub_len = strlen(sub);

  for (; (expanded_str = strstr(expanded_str, pattern));) {
    ptrdiff_t offset = expanded_str - *orig_str;
    if (sub_len > pattern_len) {
      expanded_str = realloc(*orig_str, sizeof **orig_str * (orig_str_len + sub_len - pattern_len + 1));
      if (!expanded_str) goto str_gsub_return;
      *orig_str = expanded_str;
      expanded_str = *orig_str + offset;
    }

    memmove(expanded_str + sub_len, expanded_str + pattern_len, orig_str_len + 1 - offset - pattern_len);
    memcpy(expanded_str, sub, sub_len);
    orig_str_len = orig_str_len + sub_len - pattern_len;
    expanded_str += sub_len;
  }

  expanded_str = *orig_str;
  if (sub_len < pattern_len) {
    expanded_str = realloc(*orig_str, sizeof **orig_str * (orig_str_len + 1));
    if (!expanded_str) goto str_gsub_return;
    *orig_str = expanded_str;
  }

str_gsub_return:
  return expanded_str;
}


// Main function takes no arguments:
// User will supply input in response to the command prompt
int main(void) {
  // Set some variables we will use inside the main loop
  size_t n = 0, num_tokens = 0; /* num_tokens used for tracking number of tokens/pointers in word_tokens array */
  int child_proc_status;
  pid_t shell_pid = getpid();
  pid_t child_proc_pid;
  
  char *line = NULL;
  char const *restrict ifs_var_name = "IFS";
  char **word_tokens = NULL; /* Initially set to NULL; per assignment specs, feeling fancy */
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

    // SIGNAL HANDLING TO BE IMPLEMENTED HERE
    
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
    char *dup_token;
    
    if (!token) continue; /* If there were no words to split, go back to beginning of loop and display prompt */
    dup_token = strdup(token);
    if (!dup_token) goto exit; /* Error checks strdup */
      
    word_tokens = malloc(sizeof (char **[num_tokens + 1])); /* allocate space in tokens; taken from example in Modern C */
    if (!word_tokens) goto exit; /* Error checks malloc */
    
    word_tokens[num_tokens] = dup_token;
    ++num_tokens;

    for (;;) {
      token = strtok(NULL, word_delim);
      if (!token) break;
      
      dup_token = strdup(token);
      if (!dup_token) break;

      word_tokens = realloc(word_tokens, sizeof (char **[num_tokens + 1])); /* Make sure there is enough space in word_tokens for new pointer */
      if (!word_tokens) goto exit; /* Error checks realloc? Will this lead to losing access to word_tokens if realloc fails? */
      
      word_tokens[num_tokens] = dup_token; /* Add pointer to new_token to word_tokens */
      ++num_tokens;
    }
   
    // 2. Expansion
    // String search adapted from instructor's video linked in CS344's smallsh assignment specs
    // str_gsub(**string, *pattern, *sub)
    for (size_t i = 0; i < num_tokens; ++i) {
      char *target_pattern = NULL;
      char *substitution = NULL;

      // Expand ~/ instances
      if (strcmp(&word_tokens[i][0], "~") == 0 && strcmp(&word_tokens[i][1], "/") == 0) {
        target_pattern = "~/";
        substitution = getenv("HOME");
        if (!substitution) substitution = "";
        substitution = strcat(substitution, "/");
  
        word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      }
      
      // Expand $$ instances
      target_pattern = "$$";
      char proc_pid_as_char = (char) shell_pid;
      substitution = &proc_pid_as_char;
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);

      // Expand $? instances
      target_pattern = "$?";
      int last_exit_status;
      if (waitpid(shell_pid, &last_exit_status, WNOHANG) < 0) goto exit;
      if (errno != ECHILD && errno != 0) goto exit;
      if (errno == ECHILD) {
        last_exit_status = 0;
      } else {
        last_exit_status = WIFEXITED(last_exit_status);
      }
      char exit_status_as_char = (char) last_exit_status;
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);



    } 
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
  // Based on CS344 tree assignment skeleton code
exit:
  // Free line pointer used for input
  free(line);
  for (size_t x = 0; x < num_tokens; ++x) {
    free(word_tokens[x]);
  }
  free(word_tokens);
  return errno ? -1 : 0;
}


