#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 1024


/*
 * Command struct defines a process's name, arguments, and non-default input
 * output files.
 * next points to the next command in a series of piped Commands.
 */
typedef struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin_loc;
    char *stdout_loc;
    char *stderr_loc;
    struct command *next; 
} Command;


/*
 * Returns an array of tokens from a given shell command string str.
 * Tokens:
 *	- a string inside quotation marks (token does not include quotation marks)
 *  - a word separated by space
 *  - pipe symbol |
 *  - redirect symbol < or >
 */
char ** tokenizer(char * str);

/*
 * A helper function to concatinate str1 and str2. Note that concat()
 * malloc()-s, hence the returned pointer needs to be freed
 */
char * concat(char *str1, char *str2);

/* Initializes Command struct */
Command * init_command();

/* Frees Command struct's contents from heap and frees the struct as well */
void free_command(Command *cmd);

/*
 * Frees the command linked list. Takes the head of the ll and
 * the number of elems in the linked list
 */
void free_commands(Command *cmd_ll_root, int num_cmds);

/*
 * Takes in a tokenized input, returns a linked list of Command structs
 * representing the sequence of commands specified by the user through 
 * redirects and pipes.
 */
Command * make_cmd_ll(char **tokenized, char *curr_path, int *ll_size);

/*
 * Takes a curr_path string, apointer to a linked list of Command structs
 * (and their count), and executes each one of them
 */
void exec_cmds(char *curr_path, Command *cmd, int num_cmds); 
