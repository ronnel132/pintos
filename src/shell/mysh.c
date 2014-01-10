#include <stdio.h>
#include <string.h>

#define ASCII_NUL '\0'
#define MAX_LENGTH 500

int main() {
    while (1==1) {
        char input[MAX_LENGTH];
        printf("Enter cmd\n");
        scanf("%s", input);
        printf("%s\n", input);
    }
    return 1;
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
