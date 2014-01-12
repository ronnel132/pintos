#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 1024


/* gets the pathname of cwd. from unistd.h */ 
char *getcwd(char *buf, size_t size);

char ** tokenizer(char delimiter, char * str);

typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;

int main() {
    int i, j, cmd_argc;
    char **commands, **tokenized, **cmd_argv;
    char *process, *stdin_loc, *stdout_loc;
    Command *cmd, *cur_cmd, *cmd_list_root;

    char *curr_path, *curr_user;
    
    /* get current user and path */
    curr_path = getcwd(NULL, 0);
    curr_user = getlogin();

    /* main input loop */
    while (1) {
        char input[MAX_INPUT_LENGTH];

        printf("%s:%s> ", curr_user, curr_path);
        scanf("%s", input);

        if (strcmp(input, "exit") == 0) {
            return 0;
        }

        /* Detecting whether a cd was issued. In case an argument was provided,
        *  strstr() is used together with pointer subtraction, to calculate the
        *  offset (if offset == 0, means that the substring starts at beginning of
        *  the string */
        else if ((strcmp(input, "cd") == 0) || (strstr(input, "cd ") - input == 0) || 
            (strcmp(input, "chdir ") == 0) || (strstr(input, "chdir") - input == 0)) {

            printf("chmod!\n");
        }

        printf("%s\n", input);

        commands = tokenizer('|', input);

        /* Initialize pointers to create the command-struct linked list */
        cur_cmd = NULL;
        cmd_list_root = NULL;

        i = 0;
        while (commands[i] != NULL) {
            /* The tokenized command */
            tokenized = tokenizer(' ', commands[i]);
            /* The current command we are parsing */
            cmd = (Command *) malloc(sizeof(Command));

            process = tokenized[0];
            cmd_argc = 0;
            stdin_loc = NULL;
            stdout_loc = NULL;
            j = 0;
            while (tokenized[j] != NULL) {
                /* stdin redirection specified */
                if (strcmp(tokenized[j], "<") == 0) {
                    stdin_loc = tokenized[j + 1];
                    tokenized[j] = NULL;
                    tokenized[j + 1] = NULL;
                    j = j + 2;
                }
                else if (strcmp(tokenized[j], ">") == 0) {
                    stdout_loc = tokenized[j + 1];
                    tokenized[j] = NULL;
                    tokenized[j + 1] = NULL;
                    j = j + 2;
                }
                else {
                    cmd_argc++;
                    j++;
                }
            }

            /* cmd_argc + 1 because we have a NULL pointer at the end */
            cmd_argv = (char **) malloc(sizeof(char *) * (cmd_argc + 1)); 
            j = 0;
            while (tokenized[j] != NULL) {
                cmd_argv[j] = tokenized[j];
                j++;
            }
            free(tokenized);
            cmd_argv[j] = NULL;

            /* set command struct's fields. update the cur_cmd pointer */
            cmd->process = process;
            cmd->argc = cmd_argc;
            cmd->argv = cmd_argv;
            cmd->stdin_loc = stdin_loc;
            cmd->stdout_loc = stdout_loc;
            cmd->next = NULL;

            /* append to the command-struct linked list */
            if (cur_cmd == NULL) {
                cur_cmd = cmd;
                cmd_list_root = cmd;
            }
            else {
                cur_cmd->next = cmd;
                cur_cmd = cmd;
            }
            i++;
        }
        free(commands); 
    }
    free(curr_path);
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
