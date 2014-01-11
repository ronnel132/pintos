#include <stdio.h>
#include <string.h>

#define ASCII_NUL '\0'

struct command {
    char *process;
    char **argc;
    char *stdin;
    char *stdout;
};

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
            tokens[i] = strndup(token, token_length);
            token_index++;
            token_length = 0;
            token = str_it + 1;
        }
        
        str_it++;
    }
    
    return tokens;
}
