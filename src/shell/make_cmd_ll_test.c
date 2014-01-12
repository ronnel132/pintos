#include "mysh.h"

int main() {
    char *input = "echo \"hello world\" > test.txt | mysh";
    int ll_size, i, j;
    Command *root = make_cmd_ll(input, ll_size);
    Command * iter = root;
    i = 0
    while (iter != NULL) {
        printf("Command Struct Number %d\n", i);
        printf("-process: %s\n", iter->process);
        printf("-argc: %d\n", iter->argc);
        for (j = 0; j < iter->argc; j++) {
            printf("-argv index %d: %s\n", j, iter->argv[j]);
        }
        printf("-stdin_loc: %s\n", iter->stdin_loc);
        printf("-stdout_loc: %s\n", iter->stdout_loc);
        printf("-stderr_loc: %s\n", iter->stderr_loc);
        iter = iter->next;
    }
}
