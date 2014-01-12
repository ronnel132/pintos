#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "mysh.h"

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 1024


int main() {
    char *curr_path, *curr_user;
    
    /* get current user and path */
    curr_path = getcwd(NULL, 0);
    curr_user = getlogin();

    /* main input loop */
    while (1) {
        char input[MAX_INPUT_LENGTH];

        printf("%s:%s> ", curr_user, curr_path);
        fgets(input, MAX_INPUT_LENGTH, stdin);

        if (strcmp(input, "exit") == 0) {
            return 0;
        }

        /* Detecting whether a cd was issued. In case an argument was provided,
        *  strstr() is used together with pointer subtraction, to calculate the
        *  offset (if offset == 0, means that the substring starts at beginning
        *  of the string */

        else if ((strcmp(input, "cd") == 0) || 
                (strstr(input, "cd ") - input == 0) || 
                (strcmp(input, "chdir") == 0) || 
                (strstr(input, "chdir ") - input == 0)) {

            printf("chmod!\n");
        }

        printf("%s\n", input);

        
    }
    free(curr_path);
}

char * concat(char *str1, char *str2) {
    char *buffer;
    int i, len;

    /* + 1 because of the NULL at the end */
    len = (int) (strlen(str1) + strlen(str2) + 1);

    buffer = (char*) malloc(sizeof(char *) * len);
    for (i = 0; i < (int) strlen(str1); i++) {
        buffer[i] = str1[i];
    }
    for (i = strlen(str1); i < (len - 1); i++) {
        buffer[i] = str2[i - strlen(str1)];
    }
    buffer[i] = ASCII_NUL;

    return buffer;
}

Command * make_cmd_ll(char *input, int *ll_size) {
    int i, argv_ind;
    char **tokenized, **cmd_argv;
    Command *cmd, *cur_cmd, *cmd_ll_root;

    tokenized = tokenizer(input);

    /* Initialize pointers to create the command-struct linked list */
    cur_cmd = NULL;
    cmd_ll_root = NULL;

    /* The current command we are parsing */
    cmd = (Command *) malloc(sizeof(Command));
    i = 0;
    while (tokenized[i] != NULL) {
        if (strcmp(tokenized[i], "|") == 0) {
            if (cmd_ll_root == NULL) {
                cur_cmd = cmd;
                cmd_ll_root = cmd;
            }
            else {
                cur_cmd->next = cmd;
                cur_cmd = cmd;
            }
            /* We are piping to a new command so create that command struct */
            cmd = (Command *) malloc(sizeof(Command));
            /* Initialize its fields */
            cmd->argc = 0;
            cmd->stdin_loc = NULL;
            cmd->stdout_loc = NULL;
            cmd->stderr_loc = NULL;
            cmd->next = NULL;
        }
        
        if ((i == 0) || (strcmp(tokenized[i - 1], "|"))) {
            cmd->process = tokenized[i];
        }

        if (strcmp(tokenized[i], "<")) {
            cmd->stdin_loc = tokenized[i + 1];
            i = i + 2;
        }
        else if (strcmp(tokenized[i], ">")) {
            cmd->stdout_loc = tokenized[i + 1];
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
    while (tokenized[i] != NULL) {
        if (strcmp(tokenized[i], "|") == 0) {
            cur_cmd->argv = cmd_argv;
            cur_cmd->argv[argv_ind] = NULL;
            cur_cmd = cur_cmd->next;
            cmd_argv = (char **) malloc(sizeof(char *) * 
                (cmd_ll_root->argc + 1));
            argv_ind = 0;
            *ll_size++;
        }
        if (strcmp(tokenized[i], ">") == 0 
            || strcmp(tokenized[i], ">") == 0) {
            free(tokenized[i]);
            free(tokenized[i + 1]);
            i = i + 2;
        } 
        else {
            cmd_argv[argv_ind] = tokenized[i];
            i++;
            argv_ind++;
        }
    }
    /* Set the last command struct's argv */
    cur_cmd->argv = cmd_argv;
    *ll_size++;
    
    return cmd_ll_root;
}

void exec_cmd(char *curr_path, Command *cmd, int num_cmds) {
    int i;
    pid_t pid;
    int remaining;
    int ret_val;

    /* Inspired by: 
        http://stackoverflow.com/questions/876605/multiple-child-process */

    /* Array of children pids */
    pid_t * pids = (pid_t *) malloc(num_cmds * sizeof(pid_t));
    
    for (i = 0; i < num_cmds; i++) {
        pid = fork();
        pids[i] = pid;

        if (pid < 0) {
            fputs("Fatal error: Could not fork. Aborting.", stderr);
            exit(1);
        }
        else if (pid == 0) {
            /* We're in child process here */
            execve(concat("/bin", cmd->process), cmd->argv, NULL);
            /* TODO: error handling if we're here */

            /* TODO: Fallback to exec-ing in current path */
        }

        /* Advance to next command */
        cmd = cmd->next;
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
    num_tokens = 0;

    while (*str_it != ASCII_NUL) {
        token_length++;

        /* Qutation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    num_tokens++;
                }
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;
        /* Whitespace handling */
        } else if (*str_it == ' ' || *str_it == '\t') {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                num_tokens++;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;
        /* Redirection and pipe handling */
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
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
    if (tokens == NULL) return NULL;
    tokens[num_tokens - 1] = NULL;
    

    /*
     * Populate token array.
     */

    str_it = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token = str;
    token_index = 0;

    while (*str_it != ASCII_NUL) {
        token_length++;

        /* Qutation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    tokens[token_index] = strndup(token, token_length - 1);
                    token_index++;
                }
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;
        /* Whitespace handling */
        } else if (*str_it == ' ' || *str_it == '\t') {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;
        /* Redirection and pipe handling */
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
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
