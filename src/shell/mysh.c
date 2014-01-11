#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 1024


/* gets the pathname of cwd. from unistd.h */ 
char *getcwd(char *buf, size_t size);

char ** tokenizer(char delimiter, char * str);

struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin;
    char *stdout;
    char *stderr;
    struct command *next; 
};

int main() {
    int i, j;
    char **commands, **tokenized;
    char *process, *stdin_loc, *stdout_loc;
    struct command *cmd, *cur_cmd, *cmd_list_root;

    char curr_path[MAX_CURR_PATH];
    
    /* get current path */
    getcwd(curr_path, MAX_CURR_PATH);

    /* main input loop */
    while (1) {
        char input[MAX_INPUT_LENGTH];

        printf("%s > ", curr_path);
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
            cmd = (struct command *) malloc(sizeof(struct command));

            process = tokenized[0];
            argc = 0;
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
                    argc++;
                    j++;
                }
            }

            /* argc + 1 because we have a NULL pointer at the end */
            argv = (char **) malloc(sizeof(char *) * (argc + 1)); 
            j = 0;
            while (tokenized[j] != NULL) {
                argv[j] = tokenized[j];
                j++;
            }
            free(tokenized);
            argv[j] = NULL;

            /* set command struct's fields. update the cur_cmd pointer */
            cmd->process = process;
            cmd->argc = argc;
            cmd->argv = argv;
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


char ** tokenizer(char delimiter, char * str) {
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
    in_string_char = ASCII_NUL;
    num_tokens = 1;  /* Includes NULL terminal. */
    str_it = str;
    while (*str_it != ASCII_NUL) {
        if (in_string_char) {  /* Inside a string */
            if (*str_it == in_string_char) {  /* String ending */
                in_string_char = ASCII_NUL;
            }
        } else if (*str_it == '\'' || *str_it == '"') {  /* String starting */
            in_string_char = *str_it;
        } else if (*str_it == delimiter) {  /* Not in string delimiter */
            num_tokens++;
        }
        
        str_it++;
    }

    /*
     * Allocate token array.
     */
    tokens = (char **) malloc(sizeof(char *) * num_tokens);
    tokens[num_tokens - 1] = NULL;
    
    /*
     * Populate token array.
     */
    str_it = str;
    token = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token_index = 0;
    while (*str_it != ASCII_NUL) {
        token_length++;

        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;
        } else if (*str_it == delimiter) {
            tokens[i] = strndup(token, token_length);
            token_index++;
            token_length = 0;
            token = str_it + 1;
        }
        
        str_it++;
    }
    
    return tokens;
}
