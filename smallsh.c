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
// Copied from instructor's string search function video linked in CS344's smallsh assignment specs
// Parameters: string to be searched, pattern to search for, substitution for found pattern
char *str_gsub(char *restrict *restrict orig_str, char const *restrict pattern, char const *restrict sub) {
  char *expanded_str = *orig_str;
  size_t orig_str_len = strlen(expanded_str);
  size_t const pattern_len = strlen(pattern), sub_len = strlen(sub);
  
  // if (fprintf(stderr, "Searching for %s in %s, replacing with %s\n", pattern, *orig_str, sub) < 0) goto str_gsub_return;
  for (;(expanded_str = strstr(expanded_str, pattern));) {
    // fprintf(stderr, "Pattern (%s) found (%s)\n", pattern, expanded_str);
    ptrdiff_t offset = expanded_str - *orig_str;
    if (sub_len > pattern_len) {
      // fprintf(stderr, "Realloc expanded str to %zu + %zu - %zu + 1\n", orig_str_len, sub_len, pattern_len);
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
    // fprintf(stderr, "Realloc expanded str to %zu +1\n", orig_str_len);
    expanded_str = realloc(*orig_str, sizeof **orig_str * (orig_str_len + 1));
    if (!expanded_str) {
      expanded_str = *orig_str;
      goto str_gsub_return;
    }
    *orig_str = expanded_str;
  }

  // fprintf(stderr, "Final string is %s\n", expanded_str);

str_gsub_return:
  return expanded_str;
}

/*int manage_bg_procs(pid_t group_pid, int &target_proc_status) {
  while ((target_proc_pid = waitpid(group_pid, &target_proc_status, WUNTRACED | WNOHANG | WCONTINUED) > 0)) {
    if (WIFEXITED(target_proc_status)) {
      if (fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) target_proc_status, WEXITSTATUS(target_proc_status)) < 0) goto exit_manage_bg_procs;
    } else if (WIFSIGNALED(target_proc_status)) {
      if (fprintf(stder, "Child process %jd done. Signaled %d.\n", (intmax_t) target_proc_status, WTERMSIG(target_proc_status)) < 0) goto exit_manage_bg_procs;
    } else if (WIFSTOPPED(target_proc_status)) {
      if (kill(target_proc_pid, SIGCONT) == -1) goto exit_manage_bg_procs;
      if (fprintf(stderr, "Child process %jd stopped. Continuing.\n," (intmax_t) target_proc_pid) < 0) goto exit_manage_bg_procs;
    }
  }

exit_manage_bg_procs:
  return errno ? -1 : 0;
}*/

// Main function takes no arguments:
// User will supply input in response to the command prompt
int main(void) {
  // Set some variables we will use inside the main loop
  size_t n = 0, num_tokens = 0; /* num_tokens used for tracking number of tokens/pointers in word_tokens array */
  int child_proc_status;
  pid_t shell_pid = getpid();
  pid_t child_proc_pid;
  pid_t group_pid = 0; /* For use in waitpid to get the process group for all children with calling process's process group ID */
  
  char *line = NULL;
  char const *restrict ifs_var_name = "IFS";
  char **word_tokens = NULL; /* Initially set to NULL; per assignment specs, feeling fancy */
  int last_fg_exit_status = 0; /* To hold the exit status of the last foreground process. Default is 0. */
  int last_bg_proc_pid = -1; /* To hold the PID of the most recent background process. Cast to char type for str search. Default is empty str. */
  size_t max_len_buff = 20; /*To be used with snprintf for changing number types to strings */
  // Explicitly set errno prior to doing anything
  errno = 0;
  //int main_errno = errno; /* For saving errno status of main function when needed */

  // Infinite loop to perform the shell functionality
  for (;;) {
    // 1a. Manage background processes
    // Check for any unwaited-for background processes in the same process group ID as this program
    // Loop through all child processes that have the same process group ID
    // Passing 0 to waitpid as the pid means waitpid will check all children with the current process's process group ID
    // Print correct info message to stderr for all child processes found
    while ((child_proc_pid = waitpid(group_pid, &child_proc_status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
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

    //if (manage_bg_procs(group_pid, &child_proc_status) < 0 && errno != ECHILD) goto exit;
    // Checking errno taken from Linux Programming Interface wait example, chap. 26
    if (errno != ECHILD && errno != 0) goto exit;
    // Reset errno; it is OK if errno was set to ECHILD
    // ECHILD indicates there are no child processes found
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
    // if (fprintf(stderr, "Line entered was: %s", line) < 0) goto exit;
    
    // Clear EOF and error indicators for stdin
    if (feof(stdin) != 0) {
      // Per man pages this function should not fail
      // if (fprintf(stderr, "Clearing EOF and errors. Error is %d. EOF is %d", errno, feof(stdin)) < 0) goto exit; /* DEBUGGING */ 
      clearerr(stdin);
      // fprintf(stderr, "Memset line\n");
      memset(line, 0, line_length);
      line = "exit $?";
      line_length = strlen(line);
      // fprintf(stderr, "New line is %s\n", line);
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
    
    if (!token) continue; /* If there were no words to split, go back to beginning of loop and display prompt. Num_tokens == 0 at this point. Do not reset. */
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

    // Explicitly add NULL pointer to end of word_tokens to allow for use as arg to execvp later in shell program
   /* word_tokens = realloc(word_tokens, sizeof (char **[num_tokens+1]));
    word_tokens[num_tokens+1] = NULL; */
    
    // DEBUGGING
    /* for (size_t y = 0; y < num_tokens+1; y++) {
      fprintf(stderr, "Token %zd is %s\n", y, word_tokens[y]);
    }*/
    // 2. Expansion
    // String search adapted from instructor's video linked in CS344's smallsh assignment specs
    // str_gsub(**string, *pattern, *sub)

    for (size_t i = 0; i < num_tokens; ++i) {
      char *target_pattern = NULL;
      char *substitution = NULL;

      // Expand ~/ instances
      if (word_tokens[i][0] == 126 && word_tokens[i][1] == 47) {
        target_pattern = "~/";
        substitution = getenv("HOME");
        if (!substitution) substitution = "";
        substitution = strcat(substitution, "/");
  
        word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      }
      if (errno != 0) goto exit;
      
      // Expand $$ instances
      target_pattern = "$$";
//      fprintf(stderr, "Shell PID is %jd\n", (intmax_t) shell_pid);
     char proc_pid_as_str[max_len_buff];
     memset(proc_pid_as_str, '\0', max_len_buff);
     
     if (snprintf(proc_pid_as_str, max_len_buff-1, "%jd", (intmax_t) shell_pid) < 0) goto exit;
     //if (sprintf(proc_pid_as_str, "%jd", (intmax_t) shell_pid) < 0) goto exit;

//      fprintf(stderr, "Shell PID as string is %s\n", proc_pid_as_str);
      substitution = proc_pid_as_str;
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;

      // Expand $? instances
      target_pattern = "$?";
      /*int last_exit_status;
      if (waitpid(shell_pid, &last_exit_status, WNOHANG) < 0) goto exit;
      if (errno != ECHILD && errno != 0) goto exit;
      if (errno == ECHILD) {
        last_exit_status = 0;
      } else {
        last_exit_status = WIFEXITED(last_exit_status);
      } */
      char exit_status_as_str[max_len_buff];
      memset(exit_status_as_str, '\0', max_len_buff);
      //char *exit_status_as_str = NULL;
      if (snprintf(exit_status_as_str, max_len_buff-1, "%d", last_fg_exit_status) < 0) goto exit;
      // if (sprintf(exit_status_as_str, "%d", last_fg_exit_status) < 0) goto exit;
      substitution = exit_status_as_str;
  //    fprintf(stderr, "Last exit status is %s\n", substitution);
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;

      // Expande $! instances
      target_pattern = "$!";
      if (last_bg_proc_pid == -1) {
        substitution = "";
      } else {
        char last_bg_proc_pid_str[max_len_buff];
        memset(last_bg_proc_pid_str, '\0', max_len_buff);
        // char *last_bg_proc_pid_str = NULL;
        if (snprintf(last_bg_proc_pid_str, max_len_buff-1, "%d", last_bg_proc_pid) < 0) goto exit;
        // if (sprintf(last_bg_proc_pid_str, "%d", last_bg_proc_pid) < 0) goto exit;
        substitution = last_bg_proc_pid_str;
      }
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;
    }
    
    // DEBUGGING/TESTING 
    /*for (size_t j = 0; j < num_tokens; ++j) {
      fprintf(stderr, "Expanded token is %s\n", word_tokens[j]);
    }*/
    // 4. Parsing: TO COME

    // 5. Execution
    // Insert NULL pointer at end here
    word_tokens = realloc(word_tokens, sizeof (char**[num_tokens+1]));
    word_tokens[num_tokens+1] = NULL;
    // Branch for no command word entered in stdin

    // Branch for built-in command exit
    // First word/token is command
    if (strcmp(word_tokens[0], "exit") == 0) {
      if (num_tokens > 2) {
        errno = -1;
        fprintf(stderr, "Too many arguments passed to exit command");
        goto exit;
      }
      
      // MISSING: SEND ALL CHILD PROCESSES SIGINT BEFORE EXITING
      int shell_exit_status = last_fg_exit_status;
      // No argument provided
      if (num_tokens == 1) {
        fprintf(stderr, "\nexit\n");
        exit(shell_exit_status);
      } else {
        // MISSING: ERROR IF ARG TO EXIT IS NOT AN INT
        fprintf(stderr, "\nexit\n");
        shell_exit_status = atoi(word_tokens[1]);
        exit(shell_exit_status);
      }
    } else if (strcmp(word_tokens[0], "cd") == 0) {  /* Branch for built-in command cd */
      if (num_tokens > 2) {
        errno = -1;
        fprintf(stderr, "Too many arguments passed to cd command");
        goto exit;
      }

      char *cd_arg = getenv("HOME");
      if (num_tokens == 2) {
        cd_arg = word_tokens[1];
      }
      if (chdir(cd_arg) != 0) goto exit;

    } else { /* Branch for non-built-in commands */
      // Execute non-built-in-command in a new child process
      // execvp will search PATH variable for command if command does not start with slash
      // Code below adapted from exampole code in CS344 module Process API - Executing a New Program
      int new_child_status;
      pid_t new_child_pid = fork();

      switch(new_child_pid) {
        case -1:
          goto exit; /* Error checks fork */
          break;
        case 0: /* Process is child */
          // MISSING: Handling output redirection operator and input redirection opperator
          if (execvp(word_tokens[0], word_tokens) == -1) {
            if (fprintf(stderr, "An error occurred while trying to run command %s\n", word_tokens[0]) < 0) goto exit;
            goto exit;
            // break;
          }
          break;
        default: /* Process is parent */
          if (waitpid(new_child_pid, &new_child_status, 0) == -1) goto exit; /* Blocking wait for child process with error checking */
          if (WIFSIGNALED(new_child_status)) {
            last_fg_exit_status = 128 + WTERMSIG(new_child_status);
          } else if (WIFSTOPPED(new_child_status)) {
            if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) new_child_pid) < 0) goto exit;
            kill(new_child_pid, SIGCONT);
            last_bg_proc_pid = new_child_pid;
          } else if (WIFEXITED(new_child_status)) {
            last_fg_exit_status = new_child_status; /* Set shell variable $? to exit status of this command */
          }
          break;
      }

    }
   // goto exit;
   // Free things!
   // fprintf(stderr, "Memset line\n");
   //free(line);
   memset(line, 0, line_length);
   /*for (size_t x = 0; x < num_tokens; ++x) {
     free(word_tokens[x]);
   }
   free(word_tokens);*/
   // fprintf(stderr, "Memset word_tokens\n");
   memset(word_tokens, 0, num_tokens+1);
   // fprintf(stderr, "Reset num_tokens to 0\n");
   num_tokens = 0;
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
  if (errno == 0) {
    // fprintf(stderr, "Freeing line and word_tokens if they ae not null\n");
    if (line != NULL) free(line);
    if (num_tokens > 0 && word_tokens != NULL) {
      for (size_t x = 0; x < num_tokens; ++x) {
        free(word_tokens[x]);
      }
      free(word_tokens);
    }
  }
  return errno ? -1 : 0;
}


