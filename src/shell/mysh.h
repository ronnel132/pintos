typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;

/* Takes a pointer to a linked list of Command structs, and
* executes each one of them */
void exec(Command *cmds);

Command * make_cmd_ll(char *input, int *ll_size);

char ** tokenizer(char * str);

/* Takes a curr_path string, apointer to a linked list of Command structs
* (and their count), and executes each one of them */
void exec_cmd(char *curr_path, Command *cmd, num_cmds); 
