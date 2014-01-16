typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;

/* A helper function to concatinate str1 and str2. Note that concat()
*  malloc()-s, hence the returned pointer needs to be freed
*/
char * concat(char *str1, char *str2);

/* Takes in a tokenized input, returns a linked list of Command structs
 * representing the sequence of commands specified by the user through 
 * redirects and pipes.
 */
Command * make_cmd_ll(char **tokenized, char *curr_path, int *ll_size);

Command * init_command();

void free_command(Command *cmd);

/* Frees the command linked list. Takes the head of the ll and
*  the number of elems in the linked list
*/
void free_command_struct(Command *cmd_ll_root, int num_cmds);

char ** tokenizer(char * str);

/* Takes a curr_path string, apointer to a linked list of Command structs
* (and their count), and executes each one of them */
void exec_cmds(char *curr_path, Command *cmd, int num_cmds); 
