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

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 1024

/* Main loop. Waits for user input, then parses and execs commants */
int main(void) {
    int i;
    char curr_path[PATH_MAX];
    char curr_user[LOGIN_NAME_MAX];
    struct passwd *pw;
    char *homedir;
    int chdir_status;

    char input[MAX_INPUT_LENGTH];
    char **tokenized_input;

    Command *cmd_ll;  /* The cmd linked list */
    int ll_size;  /* Size of the cmd linked list */

    /* TODO: Check for errors. */
    pw = getpwuid(getuid());
    homedir = pw->pw_dir;

    /* Loop the shell prompt, waiting for input */
    while (1) {
        /* Get current user and path */
        getcwd(curr_path, PATH_MAX);
        getlogin_r(curr_user, LOGIN_NAME_MAX);

        /* TODO: Check for errors. */

        /* Print prompt */
        printf("%s:%s> ", curr_user, curr_path);

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
                chdir_status = chdir(homedir);
            } else {
                chdir_status = chdir(tokenized_input[1]);
            }

            /* chdir changes current directory. Returns nonzero if error. */
            /* TODO: figure out why correct error messages aren't being displayed */
            if (chdir_status < 0) {
                if (errno == ENOTDIR) {
                    fputs("A component of the path is not a directory.\n", stderr);
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
            if (cmd_ll == NULL) {
                fputs("Invalid entry. Command not supported.\n", stderr);
            }
            else {
                exec_cmds(curr_path, cmd_ll, ll_size);
            }
        }

        /* Free tokenized input */
        for(i = 0; tokenized_input[i] != NULL; i++) {
            free(tokenized_input[i]);
        }
        free(tokenized_input);
    }

    /* Try to free tokenized_input again just in case we broke from the while 
     * loop above without freeing. 
     */
    for(i = 0; tokenized_input[i] != NULL; i++) {
        free(tokenized_input[i]);
    }
    free(tokenized_input);

    return 0;
}

/* A helper function to concatinate str1 and str2. Note that concat()
*  malloc()-s, hence the returned pointer needs to be freed
*/
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


Command * make_cmd_ll(char **tokenized, char *curr_path, int *ll_size) {
    int i, argv_ind;
    char **cmd_argv;
    char *curr_path_slash;
    Command *cmd, *cur_cmd, *cmd_ll_root;


    /* Initialize linked list size to 0 */
    *ll_size = 0;

    /* Create the first command and root of our command linked list */
    cmd = init_command();
    cmd_ll_root = cmd;
    cur_cmd = cmd;

    i = 0;
    while (tokenized[i] != NULL) {
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
        
        if ((i == 0) || (strcmp(tokenized[i - 1], "|") == 0)) {
            cmd->process = strdup(tokenized[i]);
        }

        if (strcmp(tokenized[i], "<") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }

            curr_path_slash = concat(curr_path, "/");
            cmd->stdin_loc = concat(curr_path_slash, tokenized[i + 1]);
            free(curr_path_slash);

            i = i + 2;
        }
        else if (strcmp(tokenized[i], ">") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }

            curr_path_slash = concat(curr_path, "/");
            cmd->stdout_loc = concat(curr_path_slash, tokenized[i + 1]);
            free(curr_path_slash);

            i = i + 2;
        }
        else {
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

    while (tokenized[i] != NULL) {
        if (strcmp(tokenized[i], "|") == 0) {
            cur_cmd->argv = cmd_argv;
            cur_cmd->argv[argv_ind] = NULL;
            cur_cmd = cur_cmd->next;
            cmd_argv = (char **) malloc(sizeof(char *) * 
                (cur_cmd->argc + 1));

            /* Malloc failed */
            if (cmd_argv == NULL) {
                fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
                exit(1);
            }

            argv_ind = 0;
            (*ll_size)++;
        }
        if (strcmp(tokenized[i], ">") == 0 
            || strcmp(tokenized[i], "<") == 0) {
            i = i + 2;
        }
        else if (strcmp(tokenized[i], "|") == 0) {
            i++;
        }
        else {
            cmd_argv[argv_ind] = strdup(tokenized[i]);
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

Command * init_command() {
    Command * cmd;
    cmd = (Command *) malloc(sizeof(Command));
    if (cmd == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    cmd->process = NULL;
    cmd->argc = 0;
    cmd->argv = NULL;
    cmd->stdin_loc = NULL;
    cmd->stdout_loc = NULL;
    cmd->stderr_loc = NULL;
    cmd->next = NULL;
    return cmd;
}

void free_command(Command *cmd) {
    int i;
    if (cmd != NULL) {
        /* Most of the command structs fields are freed in the main loop,
         * by the tokenizer. 
         */
        free(cmd->process);
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

/* Takes a curr_path string, apointer to a linked list of Command structs
* (and their count), and executes each one of them */
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
    char *tmp;

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
            /* If i is 0, then we only need to set up pipefd, because there 
             * is no previous pipe (prev_pipefd) yet.
             */
            if (i == 0) {
                if (pipe(pipefd) == -1) {
                    fputs("Fatal error: Unable to create pipe. Aborting.\n", stderr);
                    exit(1);
                }
            }
            else if (i == num_cmds - 1) {
                prev_pipefd[0] = pipefd[0];
                prev_pipefd[1] = pipefd[1];
            }
            else {
                prev_pipefd[0] = pipefd[0];
                prev_pipefd[1] = pipefd[1];
                if (pipe(pipefd) == -1) {
                    fputs("Fatal error: Unable to create pipe. Aborting.\n", stderr);
                    exit(1);
                }
            }
        }

        pid = fork();
        pids[i] = pid;

        /* Create the pipe for communication between the ith and (i + 1)th
         * processes. 
         */

        if (pid < 0) {
            fputs("Fatal error: Could not fork. Aborting.\n", stderr);
            exit(1);
        }
        else if (pid == 0) {
            /* We're in child process here */

            /* Set up stdin, stdout, stderr. Then redirect stdin/out/err.
            *  Then close these fh-s, now stdin/out/err are pointing
            *  to these locations; no need to leak fhs
            */

            /* TODO: Handle failures */

            if (cmd->stdin_loc != NULL) {
                /* Open file, read only */
                in = open(cmd->stdin_loc, O_RDWR);

                /* Set as stdin */
                dup2(in, STDIN_FILENO);

                /* Remove temporary file descriptor */
                close(in);
            }

            if (cmd->stdout_loc != NULL) {
                /*
                 * Open file for writing. Permission to create if necessary,
                 * with user read/write permissions.
                 */
                out = open(cmd->stdout_loc,
                           O_CREAT | O_RDWR,
                           S_IRUSR | S_IWUSR);

                /* Set as stdout */
                dup2(out, STDOUT_FILENO);

                /* Remove temporary file descriptor */
                close(out);
            }

            if (cmd->stderr_loc != NULL) {
                err = open(cmd->stderr_loc,
                           O_CREAT | O_RDWR,
                           S_IRUSR | S_IWUSR);

                /* Set as stderr */
                dup2(err, STDERR_FILENO);
                close(err);
            }

            if (num_cmds > 1) {
                if (i == 0) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }
                else if (i == num_cmds - 1) {
                    close(prev_pipefd[1]);
                    dup2(prev_pipefd[0], STDIN_FILENO);
                    close(prev_pipefd[0]);
                }
                else {
                    close(prev_pipefd[1]);
                    close(pipefd[0]);
                    dup2(prev_pipefd[0], STDIN_FILENO);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(prev_pipefd[0]);
                    close(pipefd[1]);
                }
            }


            /* Yeah, concat() mallocs, but we found no good way to
            *  free that pointer because of execve(). Anyway the children should
            *  exit at some point, so this "memory leak" (not really a leak)
            *  isn't really an issue in
            *  the long run, just each child will consume a few more bytes in the
            *  heap during it's lifespan.
            *  However, have to free when execve() returns, since the concat() pointers
            *  would be lost forever!
            */

            /* See if cmd is in /bin/ */
            /* Close the write end, because the child is only using the read
             * end of the pipe.
             */


            path = concat("/bin/", cmd->process);
            
            execve(path, cmd->argv, NULL);

            /* If we're here, execve failed. Let's try to run it on
            *  /usr/bin/
            */

            free(path);

            path = concat("/usr/bin/", cmd->process);

            execve(path, cmd->argv, NULL);

            /* If we're here, execve failed. Let's try to run it on
            *  current wd
            */

            free(path);

            tmp = concat(curr_path, "/");
            path = concat(tmp, cmd->process);

            execve(path, cmd->argv, NULL);

            /* If we're here, second execve failed. */
            fputs("Fatal error: Could not execve. Aborting.\n", stderr);

            free(tmp);
            free(path);

            exit(1);

            /* TODO: If one command fails, should all of them fail? */

        }
        else {
            if (num_cmds > 1) {
                if (i != 0) {
                    /* In parent we always close the pipe, it doesn't use it */
                    close(prev_pipefd[0]);
                    close(prev_pipefd[1]);
                }
            }
            /* Advance to next command */
            cmd = cmd->next;
        }
    }

    /* Wait for children (Note that we're at parent process here, since
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
    free_command_struct(cmd_ll_root, num_cmds);
}

/* Frees the command linked list. Takes the head of the ll and
*  the number of elems in the linked list
*/
void free_command_struct(Command *cmd_ll_root, int num_cmds) {
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
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
            if (token_length > 1) {
                num_tokens++;
            }
            /* Redirection or pipe symbol is its own token */
            num_tokens++;

            /* Start new token */
            token = str_it + 1;
            token_length = 0;
        }

        str_it++;
    }


    /*
     * Allocate token array.
     */

    tokens = (char **) malloc(sizeof(char *) * num_tokens);
    if (tokens == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    tokens[num_tokens - 1] = NULL;
    

    /*
     * Populate token array.
     */

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
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
            if (token_length > 1) {
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }
            /* Redirection or pipe symbol is its own token */
            tokens[token_index] = strndup(str_it, 1);
            token_index++;

            /* Start new token */
            token = str_it + 1;
            token_length = 0;
        }

        str_it++;
    }
    
    return tokens;
}
