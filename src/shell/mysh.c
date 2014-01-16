#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>

#include "mysh.h"


/* Main loop. Waits for user input, then parses and execs commands */
int main(void) {
    struct passwd *pw;  /* User info struct */

    char curr_path[PATH_MAX];
    int chdir_status;

    char input[MAX_INPUT_LENGTH];
    char **tokenized_input;

    int i;

    Command *cmd_ll;    /* The cmd linked list */
    int ll_size;        /* Size of the cmd linked list */


    /* Get user's information. */
    pw = getpwuid(getuid());
    if (pw == NULL) {
        fputs("Error retrieving user information.\n", stderr);
    }

    /* Loop the shell prompt, waiting for input */
    while (1) {
        /* Get current user and path */
        if (getcwd(curr_path, PATH_MAX) == NULL) {
            fputs("Error retrieving current directory!.\n", stderr);
            curr_path[0] = ASCII_NUL;
        }

        /* Print prompt */
        if (pw != NULL && curr_path[0] != ASCII_NUL) {
            printf("%s:%s> ", pw->pw_name, curr_path);
        } else {
            puts("> ");
        }

        /* Get user input and tokenize it */
        fgets(input, MAX_INPUT_LENGTH, stdin);
        tokenized_input = tokenizer(input);

        /* Interpret tokenized input */
        if (!tokenized_input || !*tokenized_input) {
            continue;
        } else if (strcmp(tokenized_input[0], "exit") == 0) {
            break;
        } else if (strcmp(tokenized_input[0], "cd") == 0 ||
                   strcmp(tokenized_input[0], "chdir") == 0) {

            if (tokenized_input[1] == NULL) {
                chdir_status = chdir(pw->pw_dir);
            } else {
                chdir_status = chdir(tokenized_input[1]);
            }

            /* chdir changes current directory. Returns nonzero if error. */
            if (chdir_status < 0) {
                if (errno == ENOTDIR || errno == ENOENT) {
                    fputs("A component of the path is invalid.\n", stderr);
                } else if (errno == EACCES) {
                    fputs("Access denied.\n", stderr);
                } else if (errno == EIO) {
                    fputs("IO error.\n", stderr);
                } else {
                    fputs("Error changing directory.\n", stderr);
                }
            }
        } else {
            cmd_ll = make_cmd_ll(tokenized_input, curr_path, &ll_size);
            if (cmd_ll != NULL) {
                exec_cmds(curr_path, cmd_ll, ll_size);
            }
        }

        /* Free tokenized input */
        for(i = 0; tokenized_input[i] != NULL; i++) {
            free(tokenized_input[i]);
        }
        free(tokenized_input);
    }

    /*
     * Try to free tokenized_input again just in case we broke from the while 
     * loop above without freeing. 
     */
    for(i = 0; tokenized_input[i] != NULL; i++) {
        free(tokenized_input[i]);
    }
    free(tokenized_input);

    return 0;
}


char ** tokenizer(char * str) {
    int num_tokens;
    char ** tokens;

    char * token;
    int token_length;
    
    int token_index;

    char in_string_char;
    char * str_it;


    /*
     * Count number of tokens.
     */

    str_it = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token = str;
    num_tokens = 1;

    while (1) {
        token_length++;

        /* Quotation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    num_tokens++;
                }

                token = str_it + 1;
                token_length = 0;
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;

        /* Whitespace handling */
        } else if (*str_it == ' ' ||
                   *str_it == '\t' ||
                   *str_it == '\n' ||
                   *str_it == ASCII_NUL) {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                num_tokens++;
            }

            if (*str_it == ASCII_NUL) {
                break;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;

        /* Redirection and pipe handling */
        } else if (*str_it == '<' || *str_it == '>' || *str_it == '|') {
            if (token_length > 1) {
                num_tokens++;
            }

            /* Redirection or pipe symbol is its own token */
            num_tokens++;

            /* Start new token */
            if (*str_it == '>' && *(str_it + 1) == '>') {
                /* >> is the append token so skip next char */
                token = str_it + 2;
                str_it++;
            } else {
                /* the token is just > */
                token = str_it + 1;
            }
            token_length = 0;
        }

        str_it++;
    }


    /* Allocate token array. */

    tokens = (char **) malloc(sizeof(char *) * num_tokens);
    if (tokens == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    tokens[num_tokens - 1] = NULL;
    

    /* Populate token array. */

    str_it = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token = str;
    token_index = 0;

    while (1) {
        token_length++;

        /* Quotation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    tokens[token_index] = strndup(token, token_length - 1);
                    token_index++;
                }

                token = str_it + 1;
                token_length = 0;
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;

        /* Whitespace handling */
        } else if (*str_it == ' ' ||
                   *str_it == '\t' ||
                   *str_it == '\n' ||
                   *str_it == ASCII_NUL) {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }

            if (*str_it == ASCII_NUL) {
                break;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;
        /* Redirection and pipe handling */
        } else if (*str_it == '<' || *str_it == '>' || *str_it == '|') {
            if (token_length > 1) {
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }

            /* Redirection or pipe symbol is its own token */
            if (*str_it == '>' && *(str_it + 1) == '>') {
                /* >> is the append token so it's 2 chars */
                tokens[token_index] = strndup(str_it, 2);

                /* Start new token, skip next char because >> */
                token = str_it + 2;
                str_it++;
            } else {
                tokens[token_index] = strndup(str_it, 1);

                /* Start new token */
                token = str_it + 1;
            }

            token_index++;
            token_length = 0;
        }

        str_it++;
    }
    
    return tokens;
}


char * concat(char *str1, char *str2) {
    char *buffer;
    int i, len;

    /* + 1 because of the NULL at the end */
    len = (int) (strlen(str1) + strlen(str2) + 1);

    buffer = (char*) malloc(sizeof(char *) * len);

    /* Malloc failed */
    if (buffer == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
        
    for (i = 0; i < (int) strlen(str1); i++) {
        buffer[i] = str1[i];
    }
    for (i = strlen(str1); i < (len - 1); i++) {
        buffer[i] = str2[i - strlen(str1)];
    }
    buffer[i] = ASCII_NUL;

    return buffer;
}


Command * init_command() {
    Command * cmd;

    cmd = (Command *) malloc(sizeof(Command));
    if (cmd == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }

    /* Initialize the new struct's fields */
    cmd->process = NULL;
    cmd->argc = 0;
    cmd->argv = NULL;
    cmd->stdin_loc = NULL;
    cmd->stdout_loc = NULL;
    cmd->stderr_loc = NULL;
    cmd->file_append = 0;
    cmd->next = NULL;
    return cmd;
}

void free_command(Command *cmd) {
    int i;

    if (cmd != NULL) {
        /* Free the command struct's fields. Then free the command struct. */
        free(cmd->process);

        /*
         * Iterate through command struct's argv array, and free each command.
         */
        for (i = 0; i < cmd->argc; i++) {
            free(cmd->argv[i]);
        }

        free(cmd->argv);
        free(cmd->stdin_loc);
        free(cmd->stdout_loc);
        free(cmd->stderr_loc);
        free(cmd);
    }
}

void free_commands(Command *cmd_ll_root, int num_cmds) {
    int i;
    Command *cmd;
    Command *nxt_cmd;

    cmd = cmd_ll_root;
    for (i = 0; i < num_cmds; i++) {
        nxt_cmd = cmd->next;
        free_command(cmd);
        cmd = nxt_cmd;
    }
}


Command * make_cmd_ll(char **tokenized, char *curr_path, int *ll_size) {
    int i, argv_ind;
    char **cmd_argv;
    char *curr_path_slash; 
    /* resolved_path used to store the path when using the "realpath" system 
     * call.
     */
    char resolved_path[PATH_MAX];
    Command *cmd, *cur_cmd, *cmd_ll_root;

    /* Initialize linked list size to 0 */
    *ll_size = 0;

    /* Create the first command and root of our command linked list */
    cmd = init_command();
    cmd_ll_root = cmd;
    cur_cmd = cmd;

    i = 0;
    /* Iterate through the tokens in tokenized, storing all information except
     * for the argv array, which will be computed in the next while loop, 
     * after we have calculated each command struct's argc's (so we know what 
     * size array to malloc for the argv arrays for each command struct).
     */
    while (tokenized[i] != NULL) {
        /* If we encounter a pipe, this means we should create a new command
         * struct, and add it to the end of our linked list.
         */
        if (strcmp(tokenized[i], "|") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid pipe specified.\n", stderr);
                return NULL;
            }
            /* We are piping to a new command so create that command struct */
            cmd = init_command();
            cur_cmd->next = cmd;
            cur_cmd = cmd; 
            i++; 
            continue;
        }
        
        /* Set the process name for the command struct, which is only done 
         * after a pipe or if i is zero.
         */
        if ((i == 0) || (strcmp(tokenized[i - 1], "|") == 0)) {
            cmd->process = strdup(tokenized[i]);
        }

        /* If a redirect "<" is specified, then check if the file for input 
         * exists with the realpath system call, and if so, set the command 
         * struct's stdin_loc.
         */
        if (strcmp(tokenized[i], "<") == 0) {
            if (realpath(tokenized[i + 1], resolved_path) == NULL) {
                fprintf(stderr, "Invalid file \"%s\" specified.\n", 
                    tokenized[i + 1]);
                return NULL;
            }

            curr_path_slash = concat(curr_path, "/");
            cmd->stdin_loc = concat(curr_path_slash, tokenized[i + 1]);
            free(curr_path_slash);

            /* Increment i by two, because we are not counting the tokens "<"
             * and the next token (representing the file name for redirection)
             * in counting this command struct's argc's.
             */
            i = i + 2;
        }

        /* Check if redirect token ">" is specified. If so, store this file 
         * path in the command struct's stdout_loc.
         */
        else if (strcmp(tokenized[i], ">") == 0 ||
                 strcmp(tokenized[i], ">>") == 0) {

            if (tokenized[i + 1] == NULL || 
                strcmp(tokenized[i + 1], "<") == 0 || 
                strcmp(tokenized[i + 1], ">") == 0 || 
                strcmp(tokenized[i + 1], "|") == 0) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }

            if (strcmp(tokenized[i], ">>") == 0) {
                cmd->file_append = 1;
            }

            curr_path_slash = concat(curr_path, "/");
            cmd->stdout_loc = concat(curr_path_slash, tokenized[i + 1]);
            free(curr_path_slash);

            /* Increment i by two, for the same reason as we do when handling
             * the token "<".
             */
            i = i + 2;
        }
        else {
            /* Increment i by one, as well as the command struct's argc 
             * value. 
             */
            i++;
            (cmd->argc)++;
        }
    }

    i = 0;
    argv_ind = 0;
    /* Loop for setting command structs' argv fields. */
    cur_cmd = cmd_ll_root;
    /* cmd_ll_root->argc + 1 because we include NULL at the end */
    cmd_argv = (char **) malloc(sizeof(char *) * (cmd_ll_root->argc + 1));

    /* Malloc failed */
    if (cmd_argv == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }

    /*
     * This loop iterates through the tokenized input, and set's each command
     * struct's argv array.
     */
    while (tokenized[i] != NULL) {
        /*
         * If we encounter a pipe, we should set the argv for the current 
         * command struct iterator (cur_cmd) to cmd_argv, and also set the
         * last value of this argv array to be NULL. Then, we move cur_cmd
         * to the next command struct, and malloc a new cmd_argv array.
         */
        if (strcmp(tokenized[i], "|") == 0) {
            cur_cmd->argv = cmd_argv;
            cur_cmd->argv[argv_ind] = NULL;
            cur_cmd = cur_cmd->next;
            cmd_argv = (char **) malloc(sizeof(char *) * 
                (cur_cmd->argc + 1));

            /* Malloc failed */
            if (cmd_argv == NULL) {
                fputs("Fatal error: Could not allocate memory. Aborting.\n",
                      stderr);
                exit(1);
            }
            
            /*
             * Set argv_ind to 0. This variable keeps track of our position
             * while filling the cmd_argv array for the current command
             * struct. 
             */
            argv_ind = 0;
            (*ll_size)++;
        }


        if (strcmp(tokenized[i], ">") == 0 ||
            strcmp(tokenized[i], "<") == 0) {
            /*
             * If a redirect is specified, increment iterator i by 2.
             * Don't include tokenized[i] and tokenized[i + 1] in argv.
             */
            i = i + 2;
        }
        else if (strcmp(tokenized[i], "|") == 0) {
            /*
             * If a pipe is specified, increment iterator i by 1.
             * Don't include the pipe in argv.
             */
            i++;
        }
        else {
            /* Else, store the current token in cmd_argv at index argv_ind. */
            cmd_argv[argv_ind] = strdup(tokenized[i]);

            /* Increment iterators. */
            i++;
            argv_ind++;
        }
    }

    /* Set the last command struct's argv */
    cur_cmd->argv = cmd_argv;
    cur_cmd->argv[cur_cmd->argc] = NULL;
    (*ll_size)++;
    
    return cmd_ll_root;
}


void exec_cmds(char *curr_path, Command *cmd, int num_cmds) {
    int i;

    /* Holds the PID after fork()-ing */
    pid_t pid;

    /* Number of remaining alive children */
    int remaining;

    /* Child's return value (when it's reaped) */
    int ret_val;

    /* File descriptors for stdin, stdout, and stderr */
    int in, out, err;

    /* Array to hold the two ends (fd-s) of a pipe */
    int pipefd[2];

    /* Holds the two ends of the previously used pipe */
    int prev_pipefd[2];
    Command *cmd_ll_root;

    /* Pointer to hold the concat'ed path */
    char *path;

    /* Pointer to hold the trailing / (or any temporary char * if needed */
    char *curr_path_slash;

    /* Inspired by: 
        http://stackoverflow.com/questions/876605/multiple-child-process */

    /* Store the root of the linked list. We will need this later for frees */
    cmd_ll_root = cmd;

    /* Array of children pids */
    pid_t * pids = (pid_t *) malloc(num_cmds * sizeof(pid_t));

    /* Malloc failed */
    if (pids == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    
    for (i = 0; i < num_cmds; i++) {

        /* Only set up pipes if there is more than one command. */
        if (num_cmds > 1) {
            /*
             * If i is 0, then we only need to set up pipefd, because there 
             * is no previous pipe (prev_pipefd) yet.
             */
            if (i == 0) {
                if (pipe(pipefd) == -1) {
                    fputs("Fatal error: Unable to create pipe. Aborting.\n",
                          stderr);
                    exit(1);
                }
            }
            else if (i == num_cmds - 1) {
                /*
                 * If we are at the last command, then only move the current
                 * pipe to prev_pipefd. Do NOT create a new pipe since we 
                 * won't need one.
                 */
                prev_pipefd[0] = pipefd[0];
                prev_pipefd[1] = pipefd[1];
            }
            else {
                /*
                 * Move the current pipe into the prev_pipe, then create a 
                 * new pipe with pipefd.
                 */
                prev_pipefd[0] = pipefd[0];
                prev_pipefd[1] = pipefd[1];
                if (pipe(pipefd) == -1) {
                    fputs("Fatal error: Unable to create pipe. Aborting.\n",
                          stderr);
                    exit(1);
                }
            }
        }

        pid = fork();
        pids[i] = pid;

        if (pid < 0) {
            fputs("Fatal error: Could not fork. Aborting.\n", stderr);
            exit(1);
        }
        else if (pid == 0) {
            /* We're in child process here */

            /*
             * Set up stdin, stdout, stderr. Then redirect stdin/out/err.
             * Then close these fh-s, now stdin/out/err are pointing
             * to these locations; no need to leak fhs
            */



            if (cmd->stdin_loc != NULL) {
                /* Open file, read only */
                in = open(cmd->stdin_loc, O_RDWR);

                if (in != -1) {
                    /* Set as stdin */
                    if (dup2(in, STDIN_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdin file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }

                    /* Remove temporary file descriptor */
                    close(in);
                } else {
                    fprintf(stderr,
                            "Fatal error: Unable to read %s. Aborting.\n",
                            cmd->stdin_loc);
                    exit(1);
                }
            }

            if (cmd->stdout_loc != NULL) {
                /*
                 * Open file for writing. Permission to create if necessary,
                 * with user read/write permissions.
                 */

                /* Check to see if we're dealing with a > or an >>. The former
                 * needs to create a new file, while the later needs to append
                 * to a current file
                */
                if (cmd->file_append) {
                    out = open(cmd->stdout_loc,
                               O_APPEND | O_RDWR,
                               S_IRUSR | S_IWUSR);
                } else {
                    out = open(cmd->stdout_loc,
                               O_CREAT | O_RDWR,
                               S_IRUSR | S_IWUSR);
                }
                    

                if (out != -1) {
                    /* Set as stdout */
                    if (dup2(out, STDOUT_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdout file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }

                    /* Remove temporary file descriptor */
                    close(out);
                } else {
                    fprintf(stderr,
                            "Fatal error: Unable to read/write/create %s. "
                            "Aborting.\n",
                            cmd->stdin_loc);
                    exit(1);
                }
            }

            if (cmd->stderr_loc != NULL) {
                err = open(cmd->stderr_loc,
                           O_CREAT | O_RDWR,
                           S_IRUSR | S_IWUSR);

                if (err != -1) {
                    /* Set as stderr */
                    if (dup2(err, STDERR_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stderr file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }

                    /* Remove temporary file descriptor */
                    close(err);
                } else {
                    fprintf(stderr,
                            "Fatal error: Unable to read/write/create %s. "
                            "Aborting.\n",
                            cmd->stdin_loc);
                    exit(1);
                }
            }


            /*
             * Set up piping between processes if num_cmds is greater than 
             * 1.
             */
            if (num_cmds > 1) {
                /*
                 * If we are at i = 0, we need only close pipefd's read end,
                 * then dup2 pipefd's write end to STDOUT_FILENO. 
                 */
                if (i == 0) {
                    close(pipefd[0]);
                    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdout file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }
                    close(pipefd[1]);
                }
                /*
                 * If we are at the last command (num_cmds - 1), then we are 
                 * only working with prev_pipefd (which is the pipe connected
                 * to the STDOUT of the previous command), and we dup2 the 
                 * output of prev_pipefd's read end to STDIN_FILENO, and close
                 * prev_pipefd.
                 */
                else if (i == num_cmds - 1) {
                    close(prev_pipefd[1]);
                    if (dup2(prev_pipefd[0], STDIN_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdin file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }
                    close(prev_pipefd[0]);
                }
                /*
                 * Finally, if we are at a command in-between two pipes, then
                 * we pipe between processes using BOTH prev_pipefd and 
                 * pipefd, which will pipe data from the previous process and
                 * to the next process, respectively. Thus, we dup2 
                 * prev_pipefd's read end to STDIN_FILENO, and dup2 pipefd's
                 * write end into STDOUT_FILENO. We also ensure these pipes 
                 * are closed.
                 */
                else {
                    close(prev_pipefd[1]);
                    close(pipefd[0]);
                    if (dup2(prev_pipefd[0], STDIN_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdin file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }
                    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                        fputs("Fatal error: Cannot set stdout file. "
                              "Aborting.\n", stderr);
                        exit(1);
                    }
                    close(prev_pipefd[0]);
                    close(pipefd[1]);
                }
            }


            /*
             * Yeah, concat() mallocs, but we found no good way to free that
             * pointer because of execve(). Anyway the children should
             * exit at some point, so this "memory leak" (not really a leak)
             * isn't really an issue in
             * the long run, just each child will consume a few more bytes in
             * the heap during it's lifespan.
             * However, have to free when execve() returns, since the concat()
             * pointers would be lost forever!
             */

            /* See if cmd is in /bin/ */
            path = concat("/bin/", cmd->process);
            execve(path, cmd->argv, NULL);

            /* If we're here, execve failed. Try in /usr/bin/ */
            free(path);
            path = concat("/usr/bin/", cmd->process);
            execve(path, cmd->argv, NULL);

            /* If we're here, execve failed. Try in cwd. */
            free(path);
            curr_path_slash = concat(curr_path, "/");
            path = concat(curr_path_slash, cmd->process);

            execve(path, cmd->argv, NULL);

            /* If we're here, second execve failed. */
            fputs("Fatal error: Could not execve. Aborting.\n", stderr);

            free(curr_path_slash);
            free(path);

            exit(1);

        }
        else {
            if (num_cmds > 1) {
                if (i != 0) {
                    /*
                     * In the parent process, if i != 0 (which means 
                     * prev_pipefd is not initialized yet), we close 
                     * prev_pipefd's read and write ends, because future 
                     * child processes will not need to access these IPC 
                     * channels... i.e., these pipes have served there purpose
                     * for previous processes and are no longer needed.
                     */
                    close(prev_pipefd[0]);
                    close(prev_pipefd[1]);
                }
            }
            /* Advance to next command */
            cmd = cmd->next;
        }
    }

    /*
     * Wait for children (Note that we're at parent process here, since
     * all children have exec'ed before this point
     */
    remaining = num_cmds;

    /* While there's still children alive */
    while (remaining != 0) {
        /* For each spawned child */
        for (i = 0; i < num_cmds; i++) {

            /* If children is alive */
            if (pids[i] != 0) {
                /* Wait for this child */
                waitpid(pids[i], &ret_val, 0);

                /* Set this pid to NULL to denote dead child */
                pids[i] = 0;

                /* We have one less remaining child to wait for */
                remaining--;
            }
        }
    }

    free(pids);

    /* Free the command structs */
    free_commands(cmd_ll_root, num_cmds);
}
