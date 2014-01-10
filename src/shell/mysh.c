#include <stdio.h>
#include <string.h>

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 500
#define MAX_CURR_PATH 100

int main() {
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

        printf("%s\n", input);
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
