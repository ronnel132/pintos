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

int main(void) {
    char curr_path[PATH_MAX];
    char curr_user[LOGIN_NAME_MAX];
    struct passwd *pw;
    char *homedir;

    char input[MAX_INPUT_LENGTH];
    char **tokenized_input, **tokenized_it;

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
                free(tokenized_input[1]);
                tokenized_input[1] = strdup(homedir);
            }

            /* chdir changes current directory. Returns nonzero if error. */
            if (chdir(tokenized_input[1])) {
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
            /* TODO: We should free the cmd_ll eventually */
            cmd_ll = make_cmd_ll(tokenized_input, curr_path, &ll_size);
            if (cmd_ll == NULL) {
                fputs("Invalid entry. Command not supported.\n", stderr);
            }
            else {
                exec_cmd(curr_path, cmd_ll, ll_size);
            }
        }

        /* Free tokenized input */
        tokenized_it = tokenized_input;
        while (*tokenized_it != NULL) {
            free(*tokenized_it);
            tokenized_it++;
        }
        free(tokenized_input);
    }

    return 0;
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


Command * make_cmd_ll(char **tokenized, char *curr_path, int *ll_size) {
    int i, argv_ind;
    char **cmd_argv;
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
            cmd->stdin_loc = concat(concat(curr_path, "/"), strdup(tokenized[i + 1]));
            i = i + 2;
        }
        else if (strcmp(tokenized[i], ">") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }
            cmd->stdout_loc = concat(concat(curr_path, "/"), strdup(tokenized[i + 1]));
            i = i + 2;
        }
        else {
            i++;
            cmd->argc++;
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
                (cmd_ll_root->argc + 1));

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
    if (cmd != NULL) {
        /* Most of the command structs fields are freed in the main loop,
         * by the tokenizer. 
         */
        free(cmd->argv[cmd->argc]); // free the NULL at the end
        free(cmd->argv);
        free(cmd);
    }
}

void exec_cmd(char *curr_path, Command *cmd, int num_cmds) {
    int i;
    pid_t pid;
    int remaining;
    int ret_val;
    int in, out, err;
    /* Array of pipe file descriptor arrays. */
    int **pipefds;
    Command *cmd_ll_root, *nxt_cmd;

    /* Inspired by: 
        http://stackoverflow.com/questions/876605/multiple-child-process */

    /* Store the root of the linked list. We will need this later for frees */
    cmd_ll_root = cmd;

    if (num_cmds > 1) {
        /* Initialize the pipefds variable with new pipes. */
        // Number of will be num_cmds - 1: One pipe between each pair adjacent pair of commands.
        pipefds = (int **) malloc(sizeof(int *) * (num_cmds - 1));

        if (pipefds == NULL) {
            fputs("Failure to create pipes. Aborting.\n", stderr);
            exit(1);
        }

        for (i = 0; i < (num_cmds - 1); i++) {
            pipefds[i] = (int *) malloc(sizeof(int) * 2);
            if (pipefds[i] == NULL) {
                fputs("Failure to create pipe. Aborting.\n", stderr);
                exit(1);
            }
            if (pipe(pipefds[i]) == -1) {
                fputs("Failure to create pipe. Aborting.\n", stderr);
                exit(1);
            }
        }
    }

    /* Array of children pids */
    pid_t * pids = (pid_t *) malloc(num_cmds * sizeof(pid_t));

    /* Malloc failed */
    if (pids == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    
    for (i = 0; i < num_cmds; i++) {
        pid = fork();
        pids[i] = pid;
        
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

            /* Child should close the write end of the pipe */
            /* TODO: Properly manage pipes. We don't need to close pipes that
            *  were never opened, and we need to close ALL relevant handlers */
            close(pipefds[i][1]);

            /* TODO: Handle failures */
            /* TODO: Check created file permissions */

            if (cmd->stdin_loc != NULL) {
                in = open(cmd->stdin_loc, O_RDONLY);
                dup2(in, STDIN_FILENO);
                close(in);
            }

            if (cmd->stdout_loc != NULL) {
                out = open(cmd->stdout_loc, O_CREAT | O_RDWR);
                dup2(out, STDOUT_FILENO);
                close(out);
            }

            if (cmd->stderr_loc != NULL) {
                err = open(cmd->stderr_loc, O_CREAT | O_RDWR);
                dup2(err, STDERR_FILENO);
                close(err);
            }

            /* Set up piping between commands */
            if (num_cmds > 1) {
                if ((i == 0) && cmd->stdout_loc == NULL) {
                    dup2(pipefds[0][1], STDOUT_FILENO);
                    close(pipefds[0][1]);
                }
                else if ((i == num_cmds - 1) && cmd->stdin_loc == NULL) {
                    dup2(pipefds[num_cmds - 2][0], STDIN_FILENO);
                    close(pipefds[num_cmds - 2][0]);
                }
                else {
                    /* Since this command is in-between two pipes, it's STDIN 
                     * and STDOUT are provided by the commands before and
                     * after this one.
                     */
                    if (cmd->stdin_loc == NULL && cmd->stdout_loc == NULL) {
                        dup2(pipefds[i - 1][0], STDIN_FILENO);
                        close(pipefds[i - 1][0]);

                        dup2(pipefds[i][1], STDOUT_FILENO);
                        close(pipefds[i][1]);
                    }
                }
            }

            /* See if cmd is in /bin/ */
            /* Close the write end, because the child is only using the read
             * end of the pipe.
             */
            
            execve(concat("/bin/", cmd->process), cmd->argv, NULL);

            /* If we're here, execve failed. Let's try to run it on
            *  /usr/bin/
            */

            execve(concat("/usr/bin/", cmd->process), cmd->argv, NULL);

            /* If we're here, execve failed. Let's try to run it on
            *  current wd
            */

            execve(concat(concat(curr_path, "/"), cmd->process), cmd->argv, NULL);

            /* If we're here, second execve failed. */
            fputs("Fatal error: Could not execve. Aborting.\n", stderr);
            exit(1);

            /* TODO: If one command fails, should all of them fail? */

        }
        /* We're in the parent process here */
        /* Close the read end, because the parent process only writes to
         * the pipe.
         */
        else {
            close(pipefds[0][0]);

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

    /* Free the command structs */
    cmd = cmd_ll_root;
    for (i = 0; i < num_cmds; i++) {
        nxt_cmd = cmd->next;
        free_command(cmd);
        cmd = nxt_cmd;
    }

    /* Free the pipe array */
    if (num_cmds > 1) {
        for (i = 0; i < (num_cmds - 1); i++) {
            free(pipefds[i]);
        }
        free(pipefds);
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
