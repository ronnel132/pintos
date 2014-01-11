#include <stdio.h>
#include <string.h>

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 100

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

    /* current path */
    char curr_path[MAX_CURR_PATH] = "/";

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

        commands = tokenizer("|", input);
        
        if (commands[1] == NULL) {
            /* Single one-word command. Try the built-in processes first */
            // TODO
        }

        /* Initialize pointers to create the command-struct linked list */
        cur_cmd = NULL;
        cmd_list_root = NULL;

        i = 0;
        while (commands[i] != NULL) {
            /* The tokenized command */
            tokenized = tokenizer(" ", commands[i]);
            /* The current command we are parsing */
            cmd = (struct command *) malloc(sizeof(struct command));

            process = tokenized[0];
            argc = 0;
            stdin_loc = NULL;
            stdout_loc = NULL;
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
                }
            }

            /* argc + 1 because we have a NULL pointer at the end */
            argv = (char **) malloc(sizeof(char *) * (argc + 1)); 
            j = 0;
            while (tokenized[j] != NULL) {
                argv[j] = tokenized[j];
                j++;
            }
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
        }
    }
}

char ** tokenizer(char token, char * str) {
    char ** tokens;
    int num_tokens = 0;
    
    while (*str != ASCII_NUL) {
        if (*str == token) {
            if (num_tokens == 0) {
                num_tokens++;
            }
            num_tokens++;
        }
        
        str++;
    }
    
    /* num_tokens + 1 because tokens will be null terminated */
    tokens = (char **) malloc(sizeof(char *) * (num_tokens + 1));
    
    // todo
}
