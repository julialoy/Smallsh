#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> /* For pid_t type */
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>


/* Free word_tokens function adapted from free_file_list function in CS344 tree assignment skeleton code
* Parameters: char **word_token_array (array of pointers to word tokens created by strtok)
*             size_t token_num (number of tokens created)
* Returns:    no return value */
static void free_word_tokens(char **word_token_array, size_t token_num) {
  for (size_t i = 0; i < token_num; i++) {
    free(word_token_array[i]);
  }
  free(word_token_array);
}

/* String search function copied from instructor's string search function video
* Parameters: char *restrict *restrict orig_str (string to be searched), 
*             char const *restrict pattern (substring to search for), 
*             char const *restruct sub (substitution string for found pattern)
* Returns:    char *expanded_str (contains orig_str with the pattern replaced or is orig_str if pattern not found) */
char *str_gsub(char *restrict *restrict orig_str, char const *restrict pattern, char const *restrict sub) {
  char *expanded_str = *orig_str;
  size_t orig_str_len = strlen(expanded_str);
  size_t const pattern_len = strlen(pattern), sub_len = strlen(sub);
  
  // If pattern exists in expanded_str, replace
  for (;(expanded_str = strstr(expanded_str, pattern));) {
    ptrdiff_t offset = expanded_str - *orig_str; /* Track current place in expanded_str */
    if (sub_len > pattern_len) {
      expanded_str = realloc(*orig_str, sizeof **orig_str * (orig_str_len + sub_len - pattern_len + 1));
      if (!expanded_str) goto str_gsub_return; /* Error check realloc */
      *orig_str = expanded_str;
      expanded_str = *orig_str + offset;
    }
    
    // Replace pattern with substitution and update orig_str_len to correct length
    memmove(expanded_str + sub_len, expanded_str + pattern_len, orig_str_len + 1 - offset - pattern_len);
    memcpy(expanded_str, sub, sub_len);
    orig_str_len = orig_str_len + sub_len - pattern_len;
    expanded_str += sub_len;
  }

  expanded_str = *orig_str;
  if (sub_len < pattern_len) {
    expanded_str = realloc(*orig_str, sizeof **orig_str * (orig_str_len + 1));
    if (!expanded_str) {  /* Error check realloc */
      expanded_str = *orig_str;
      goto str_gsub_return;
    }
    *orig_str = expanded_str;
  }

str_gsub_return:
  return expanded_str;
}

int main(void) {
  // Variables that must maintain value and access outside of main loop
  size_t max_len_buff = 30; /* For creating a large enough buffer as needed */
  size_t n = 0; /* For holding allocated size of line var */
  size_t num_tokens = 0; /* Tracks number of tokens/pointers in word_tokens array */
  pid_t shell_pid = getpid(); /* pid of smallsh */
  pid_t last_bg_proc_pid = 0; /* pid of most recent background process. Used for $! expansion. */
  int last_fg_exit_status = 0; /* Exit status of last-run foreground process. Used for $? expansion.  Default is 0. */

  // int child_proc_status;
  // pid_t child_proc_pid;
  // pid_t group_pid = 0; /* For use in waitpid to get the process group for all children with calling process's process group ID */
  char const *restrict ifs_str = "IFS"; /* Environment variable name for getting value of delimiter characters. Used for word splitting. */
  char const *restrict prompt_str = "PS1"; /* Environment variable name for getting value of PS1 variable. Used for PS1 expansino. */
  char *line = NULL; /* Stores result of command line passed to smallsh */
  char **word_tokens = NULL; /* Stores pointers to words/tokens. Used for */

  // Explicitly set errno to 0 at start
  errno = 0;

  // Infinite loop performs shell functionality
  for (;;) {
    // Variables for managing background processes
    // Do not need to persist on subsequent loop iterations
    // pid_t child_proc_pid = 0; /* To hold pid of unwaited-for background process */
    // int child_proc_status = 0; /* To hold exit status or signal of unwaited-for backgorund process*/
    
    // 1a. Manage background processes: Check for any unwaited-for background processes in the same process group ID as smallsh
    /* while ((child_proc_pid = waitpid(0, &child_proc_status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
      // Use of macros based on CS344 modules and Linux Programming Interface text
      if (WIFEXITED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_proc_pid, WEXITSTATUS(child_proc_status)) < 0) goto exit;
      } else if (WIFSIGNALED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_proc_pid, WTERMSIG(child_proc_status)) < 0) goto exit;
      } else if (WIFSTOPPED(child_proc_status)) {
        if (kill(child_proc_pid, SIGCONT) == -1) err(errno, "Unable to send SIGCONT signal");
        if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_proc_pid) < 0) goto exit;
      }
    }
    
    // Checking errno taken from Linux Programming Interface wait example, chap. 26
    // ECHILD indicates there were no unwaited for processes
    if (errno != ECHILD && errno != 0) goto exit;
    // Reset errno to remove ECHILD error if it exists
    errno = 0;*/
    
    //1b. PS1 expansion/prompt display
    char *prompt = NULL; /* Prompt should not persist through loop iterations. Retrieve on each iteration. */
    char *temp_prompt = getenv(prompt_str); /* Avoid accidentally overwriting PS1 value. */
    if (!temp_prompt) {
      prompt = ""; /* Set prompt to default value if PS1 is an empty string */
    } else {
      prompt = temp_prompt;
    }
    if (fprintf(stderr, "%s", prompt) < 0) goto exit;
    
    //1c. Read a line of input from stdin
    // Code copied from example in smallsh assignment specs
    ssize_t line_length = getline(&line, &n, stdin);
    if (line_length == -1 && errno != EOF) goto exit;
    
    // Clear EOF and error indicators for stdin if EOF encountered
    if (feof(stdin) != 0) {
      clearerr(stdin); /* Per man pages clearerr should not fail */
      free(line); // ?
      n = 0;
      // line = "exit $?"; /* EOF on stdin interpreted as exit $? command */
      // line_length = strlen(line);
      goto exit;
    }

    // Reset errno in case EOF was encountered when reading from stdin
    errno = 0;

    // SIGNAL HANDLING TO BE IMPLEMENTED HERE
    
    //2. Word splitting
    // Get IFS environment variable value and set delimiter for use in strtok
    char *word_delim = NULL; /* Delimiter should not persist through loop iterations. */
    char *temp_delim = getenv(ifs_str); /* Avoid accidentally overwriting IFS value. */
    if (!temp_delim) {
      word_delim = "\t\n"; /* Set delimiter to default of IFS is null */
    } else {
      word_delim = temp_delim;
    }

    // Split line into words on the given delimiters
    char *token = strtok(line, word_delim);
    char *dup_token = NULL;
    
    if (!token) continue; /* If there were no words to split, go back to beginning of loop and display prompt. Num_tokens == 0 at this point. Do not reset. */
    dup_token = strdup(token);
    if (!dup_token) goto exit; /* Error check strdup. No need to free dup_token on failure. */
      
    word_tokens = malloc(sizeof (char **[num_tokens + 1])); /* allocate space in tokens; taken from example in Modern C */
    if (!word_tokens) { /* Error check malloc and free dup_token if error exists */
      free(dup_token);
      goto exit;
    }
    
    word_tokens[num_tokens] = dup_token;
    num_tokens++;

    for (;;) {
      token = strtok(NULL, word_delim);
      if (!token) break;
      
      dup_token = strdup(token);
      if (!dup_token) goto exit;

      word_tokens = realloc(word_tokens, sizeof (char **[num_tokens + 1])); /* Make sure there is enough space in word_tokens for new pointer */
      if (!word_tokens) {
        free(dup_token);
        goto exit; /* Error checks realloc? Will this lead to losing access to word_tokens if realloc fails? */
      }

      word_tokens[num_tokens] = dup_token; /* Add pointer to new_token to word_tokens */
      num_tokens++;
    }

    // 2. Environment Variable Expansion
    for (size_t i = 0; i < num_tokens; ++i) {
      char *target_pattern = NULL;
      char *substitution = NULL;
      char *temp_sub = NULL;
      // Expand ~/ instances only if ~/ appears at beginning of word
      if (word_tokens[i][0] == 126 && word_tokens[i][1] == 47) {
        target_pattern = "~";
        temp_sub = getenv("HOME");

        if (!temp_sub) {
          substitution = "";
        } else {
          substitution = strdup(temp_sub);
        }
        word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      
        free(substitution);
        if (errno != 0) goto exit;
      }

      // Expand $$ instances
      target_pattern = "$$";
      char proc_pid_as_str[max_len_buff];
      memset(proc_pid_as_str, '\0', max_len_buff);
      if (snprintf(proc_pid_as_str, max_len_buff-1, "%jd", (intmax_t) shell_pid) < 0) goto exit;
      substitution = proc_pid_as_str;
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;

      // Expand $? instances
      target_pattern = "$?";
      char exit_status_as_str[max_len_buff];
      memset(exit_status_as_str, '\0', max_len_buff);
      if (snprintf(exit_status_as_str, max_len_buff-1, "%d", last_fg_exit_status) < 0) goto exit;
      substitution = exit_status_as_str;
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;

      // Expand $! instances
      target_pattern = "$!";
      if (last_bg_proc_pid == 0) {
        substitution = "";
      } else {
        char last_bg_proc_pid_str[max_len_buff];
        memset(last_bg_proc_pid_str, '\0', max_len_buff);
        if (snprintf(last_bg_proc_pid_str, max_len_buff-1, "%d", last_bg_proc_pid) < 0) goto exit;
        substitution = last_bg_proc_pid_str;
      }
      word_tokens[i] = str_gsub(&word_tokens[i], target_pattern, substitution);
      if (errno != 0) goto exit;
    }
    
    // 4. Parsing
    // Remove comments
    for (size_t j = 0; j < num_tokens; j++) {
      if (strcmp(word_tokens[j], "#") == 0) {
        size_t token_count_diff = 0;
        for (size_t n = j; n < num_tokens; n++) {
          free(word_tokens[n]);
          word_tokens[n] = NULL;
          token_count_diff++;
        }
        num_tokens -= token_count_diff;
        break;
      }
    }
    
    size_t input_ptr_offset = 0;
    size_t output_ptr_offset = 0;
    int is_bg_proc = false;
    // Determine whether command will run in background
    if (strcmp(word_tokens[num_tokens-1], "&") == 0) {
      free(word_tokens[num_tokens-1]);
      word_tokens[num_tokens-1] = NULL;
      is_bg_proc = true;
      num_tokens--;
    }

    // Determine whether stdin/stdout will be redirected 
    for (size_t j = num_tokens-1; j > 0; --j) {
      if (num_tokens > 2 && strcmp(word_tokens[j], "<") == 0 && (j == (num_tokens - 2) || j == (num_tokens - 4))) {
        if (strcmp(word_tokens[j+1], "<") != 0 && strcmp(word_tokens[j+1], ">") != 0 && strcmp(word_tokens[j+1], "&") != 0) {
          input_ptr_offset = j;
        }
        // Do input redirection
      } else if (num_tokens > 2 && strcmp(word_tokens[j], ">") == 0 && (j == (num_tokens - 2) || j == (num_tokens - 4))) {
        if (strcmp(word_tokens[j+1], "<") != 0 && strcmp(word_tokens[j+1], ">") != 0 && strcmp(word_tokens[j+1], "&") != 0) {
          output_ptr_offset = j;
        }
      }
    }
    
    char *input_file = NULL;
    char *output_file = NULL;
    // If input/output will be redirected, save that info and remove it from the word_tokens array along with the redirect operator
    if (input_ptr_offset != 0) {
      input_file = strdup(word_tokens[input_ptr_offset+1]);
      free(word_tokens[input_ptr_offset+1]);
      word_tokens[input_ptr_offset+1] = NULL;
      free(word_tokens[input_ptr_offset]);
      word_tokens[input_ptr_offset] = NULL;
      num_tokens -= 2;
    }

    if (output_ptr_offset != 0) {
      output_file = strdup(word_tokens[output_ptr_offset+1]);
      free(word_tokens[output_ptr_offset+1]);
      word_tokens[output_ptr_offset+1] = NULL;
      free(word_tokens[output_ptr_offset]);
      word_tokens[output_ptr_offset] = NULL;
      num_tokens -= 2;
    }

    // 5. Execution
    // Insert NULL pointer at end here
    word_tokens = realloc(word_tokens, sizeof (char **[num_tokens+1]));
    word_tokens[num_tokens] = NULL;
    // Branch for no command word entered in stdin

    // Branch for built-in command exit
    // First word/token is command
    if (strcmp(word_tokens[0], "exit") == 0) {
      if (num_tokens > 2) {
        errno = -1;
        fprintf(stderr, "Too many arguments passed to exit command\n");
        goto exit;
      }
      
      // MISSING: SEND ALL CHILD PROCESSES SIGINT BEFORE EXITING
      int shell_exit_status = last_fg_exit_status;
      if (num_tokens == 2) {
        char *exit_status_str = NULL;
        size_t arg_len = strlen(word_tokens[1]);
        exit_status_str = malloc(sizeof (size_t) * arg_len);
        for (size_t c = 0; c < arg_len; c++) {
          if (isdigit(word_tokens[1][c]) != 0) {
            exit_status_str[c] = word_tokens[1][c];
          } else {
            fprintf(stderr, "Exit status arg contains non-digits: %s\n", word_tokens[1]); /* No error checking since exit immediately */
            free(exit_status_str);
            goto exit;
          }
        }

        shell_exit_status = atoi(exit_status_str);
        if (shell_exit_status < 0) {
          errno = -1;
          goto exit;
        }
        last_fg_exit_status = shell_exit_status;
        free(exit_status_str);
        fprintf(stderr, "\nexit\n");
        // fprintf(stderr, "Exit status is %d\n", shell_exit_status);
        exit(shell_exit_status);
      } else if (num_tokens == 1) {
        fprintf(stderr, "\nexit\n");
        // fprintf(stderr, "Exit status is %d\n", shell_exit_status);
        exit(shell_exit_status);
      } else {
        // MISSING: ERROR IF ARG TO EXIT IS NOT AN INT
        fprintf(stderr, "\nexit\n");
        // shell_exit_status = atoi(word_tokens[1]);
        exit(shell_exit_status);
      }
    } else if (strcmp(word_tokens[0], "cd") == 0) {  /* Branch for built-in command cd */
      if (num_tokens > 2) {
        errno = -1;
        fprintf(stderr, "Too many arguments passed to cd command\n");
        goto exit;
        // continue;
        //goto reset;
      }

      char *cd_arg = NULL;
      char *temp_cd_arg = getenv("HOME");
      if (num_tokens == 2) {
        cd_arg = word_tokens[1];
      } else {
        cd_arg = temp_cd_arg;
      }
      
      if (chdir(cd_arg) != 0) {
        fprintf(stderr, "Unable to change directory\n");
        // continue;
        //goto reset;
        goto exit;
      }

    } else { /* Branch for non-built-in commands */
      // Execute non-built-in-command in a new child process
      // execvp will search PATH variable for command if command does not start with slash
      // Code below adapted from exampole code in CS344 module Process API - Executing a New Program
      int new_child_status;
      pid_t new_child_pid = fork();

      switch(new_child_pid) {
        case -1:
          fprintf(stderr, "An error occurred in fork()\n");
          goto exit; /* Error checks fork */
          break;
        case 0: /* Process is child */
          // MISSING: Handling output redirection operator and input redirection opperator
          /* Redirection handling adapted from Linux Programming Interface section 27.4 example code */
          if (input_file) {
            int in_fd;
            in_fd = open(input_file, O_RDONLY);
            if (in_fd == -1) {
             fprintf(stderr, "An error occurred while trying to redirect stdin to %s\n", input_file);
             goto exit;
            }

            if (in_fd != STDIN_FILENO) {
              if (dup2(in_fd, STDIN_FILENO) == -1) {
               fprintf(stderr, "An error occurred while trying to redirect stdin to %s\n", input_file);
               goto exit;
              }
              if (in_fd != 0) {
                close(in_fd);
              }
            }
          }

          if (output_file) {
           // fprintf(stderr, "Redirect output\n");
            int out_fd;
            out_fd = open(output_file, O_CREAT | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
            if (out_fd == -1) {
              fprintf(stderr, "An error occurred while trying to redirect stdout to %s\n", output_file);
              goto exit;
            }

            if (out_fd != STDOUT_FILENO) {
              if (dup2(out_fd, STDOUT_FILENO) == -1) {
                fprintf(stderr, "An error occurred while trying to redirect stdout to %s\n", output_file);
                goto exit;
              }
              if (out_fd != 1) {
                close(out_fd);
              }
            }
          }
          
          // Execute new command
          if (execvp(word_tokens[0], word_tokens) == -1) {
            if (fprintf(stderr, "An error occurred while trying to run command %s\n", word_tokens[0]) < 0) goto exit;
            goto exit;
          }
          fflush(stdin);
          fflush(stdout);
          fflush(stderr);
          break;
        default: /* Process is parent */
          // Perform blocking wait if process is not run in background
          fflush(stdin);
          fflush(stdout);
          fflush(stderr);
          if (!is_bg_proc) {
            // fprintf(stderr, "Wait for child process %jd\n", (intmax_t) new_child_pid);
            pid_t waitpid_return_val = waitpid(new_child_pid, &new_child_status, 0);
            if (waitpid_return_val == -1) goto exit;
            // if (waitpid(new_child_pid, &new_child_status, 0) == -1) goto exit; /* Blocking wait for child process with error checking */
            // Set shell variable $?
            //if (!new_child_status) {
              //fprintf(stderr, "An error occurred in waitpid. Child %jd status is %d\n", (intmax_t) waitpid_return_val, new_child_status);
              //goto exit;
            //}

            // last_fg_exit_status = new_child_status;
            if (WIFSIGNALED(new_child_status)) {
              last_fg_exit_status = 128 + WTERMSIG(new_child_status);
            } else if (WIFSTOPPED(new_child_status)) {
              if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) new_child_pid) < 0) goto exit;
              if (kill(new_child_pid, SIGCONT) == -1) {
                fprintf(stderr, "Unable to send SICONT to child %jd\n", (intmax_t) new_child_pid);
                goto exit;
              }
              last_bg_proc_pid = new_child_pid;
            } else if (WIFEXITED(new_child_status)) {
              last_fg_exit_status = WEXITSTATUS(new_child_status);
            }
          } else { /* Do not wait for background process */
            last_bg_proc_pid = new_child_pid;
          }
          break;
      }

    }

    // Variables for managing background processes
    // Do not need to persist on subsequent loop iterations
    pid_t child_proc_pid = 0; /* To hold pid of unwaited-for background process */
    int child_proc_status = 0; /* To hold exit status or signal of unwaited-for backgorund process*/
    
    // 1a. Manage background processes: Check for any unwaited-for background processes in the same process group ID as smallsh
    while ((child_proc_pid = waitpid(0, &child_proc_status, WUNTRACED | WNOHANG | WCONTINUED)) > 0) {
      // Use of macros based on CS344 modules and Linux Programming Interface text
      if (WIFEXITED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) child_proc_pid, WEXITSTATUS(child_proc_status)) < 0) goto exit;
      } else if (WIFSIGNALED(child_proc_status)) {
        if (fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) child_proc_pid, WTERMSIG(child_proc_status)) < 0) goto exit;
      } else if (WIFSTOPPED(child_proc_status)) {
        if (kill(child_proc_pid, SIGCONT) == -1) err(errno, "Unable to send SIGCONT signal");
        if (fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) child_proc_pid) < 0) goto exit;
      }
    }
    
    // Checking errno taken from Linux Programming Interface wait example, chap. 26
    // ECHILD indicates there were no unwaited for processes
    if (errno != ECHILD && errno != 0) goto exit;
    // Reset errno to remove ECHILD error if it exists
    errno = 0;

    // Free word_tokens and line at end of each loop iteration to avoid memory leaks
    if (output_file != NULL) free(output_file);

    if (input_file != NULL) free(input_file);
    free_word_tokens(word_tokens, num_tokens);
    if (n > 0 && line != NULL) free(line);
    // Reset num_tokens and n for next loop iteration
    num_tokens = 0;
    n = 0;
  }

exit:
  // Free word_tokens and line if needed
  if (num_tokens > 0 && word_tokens != NULL) {
    free_word_tokens(word_tokens, num_tokens);
  }
  if (n > 0 && line != NULL) {
    free(line);
  }
  // Returning errno or 0 depending on if errno is set copied from CS344's tree assignment skeleton code
  return errno ? -1 : 0;
}

