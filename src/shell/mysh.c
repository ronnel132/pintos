#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 1024


/* gets the pathname of cwd. from unistd.h */ 
char *getcwd(char *buf, size_t size);

typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;

Command * make_cmd_ll(char *input, int *ll_size);
char ** tokenizer(char delimiter, char * str);

int main() {
    
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

        
    }
    free(curr_path);
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
            tokens[token_index] = strndup(token, token_length);
            token_index++;
            token_length = 0;
            token = str_it + 1;
        }
        
        str_it++;
    }
    
    return tokens;
}
