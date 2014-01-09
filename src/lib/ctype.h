/*! \file ctype.h
 *
 * Inline functions to classify different kinds of characters.
 * All classifications are in the US-ASCII character set; these
 * functions do not support different locales.
 */

#ifndef __LIB_CTYPE_H
#define __LIB_CTYPE_H

/*! Returns nonzero if c is a lowercase character, 0 otherwise. */
static inline int islower(int c) { return c >= 'a' && c <= 'z'; }

/*! Returns nonzero if c is an uppercase character, 0 otherwise. */
static inline int isupper(int c) { return c >= 'A' && c <= 'Z'; }

/*! Returns nonzero if c is an alphabetical character, 0 otherwise. */
static inline int isalpha(int c) { return islower (c) || isupper (c); }

/*! Returns nonzero if c is a base-10 digit (0-9), 0 otherwise. */
static inline int isdigit(int c) { return c >= '0' && c <= '9'; }

/*! Returns nonzero if c is an alphanumeric character (either letter or digit),
 *  0 otherwise.
 */
static inline int isalnum(int c) { return isalpha (c) || isdigit (c); }

/*! Returns nonzero if c is a hexadecimal digit (0-9, or a-f, or A-F),
 *  0 otherwise.
 */
static inline int isxdigit(int c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/*! Returns nonzero if c is a whitespace character, 0 otherwise. */
static inline int isspace(int c) {
    return (c == ' ' || c == '\f' || c == '\n'
            || c == '\r' || c == '\t' || c == '\v');
}

/*! Returns nonzero if c is a space or a tab, 0 otherwise. */
static inline int isblank(int c) { return c == ' ' || c == '\t'; }

static inline int isgraph(int c) { return c > 32 && c < 127; }
static inline int isprint(int c) { return c >= 32 && c < 127; }
static inline int iscntrl(int c) { return (c >= 0 && c < 32) || c == 127; }
static inline int isascii(int c) { return c >= 0 && c < 128; }

/*! Returns nonzero if c is a punctuation character, 0 otherwise.  Note
 * that a character is considered "punctuation" if it is a printable
 * character, and is not alphanumeric or a whitespace character.
 */
static inline int ispunct(int c) {
    return isprint(c) && !isalnum(c) && !isspace(c);
}

/*! If c is an alphabetical character, converts it to lowercase, otherwise
 *  returns c unchanged.
 */
static inline int tolower(int c) { return isupper(c) ? c - 'A' + 'a' : c; }

/*! If c is an alphabetical character, converts it to uppercase, otherwise
 *  returns c unchanged.
 */
static inline int toupper(int c) { return islower(c) ? c - 'a' + 'A' : c; }

#endif /* lib/ctype.h */

