typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;

/* Concatenates two strings str1 and str2 */
char * concat(char *str1, char *str2);

/* Takes in a tokenized input, returns a linked list of Command structs
 * representing the sequence of commands specified by the user through 
 * redirects and pipes.
 */
Command * make_cmd_ll(char **tokenized, int *ll_size);

char ** tokenizer(char * str);

/* Takes a curr_path string, apointer to a linked list of Command structs
* (and their count), and executes each one of them */
void exec_cmd(char *curr_path, Command *cmd, int num_cmds); 
