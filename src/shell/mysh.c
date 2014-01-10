#include <stdio.h>
#include <string.h>

#define ASCII_NUL '\0'

struct command {
    char *process;
    int argc;
    char **argv;
    char *stdin;
    char *stdout;
    char *stderr;
};

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
