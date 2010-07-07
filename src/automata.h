#ifndef _H_AUTOMATA
#define _H_AUTOMATA

/* Some useful constants */

#include <stdlib.h>
#define BOOL int
#include "configbase.h" // some constants
#ifndef __AUTOMATA_INTERNALS
#define AUTOMATA void *
#endif // __AUTOMATA_INTERNALS

/*  Only ascii automata implemented so far (Matching limited to characters
    from 0 to 127) I use it for URL and domain matching.

    For each function two parts implemented. Forward matching is better for
    finding prefixes, and backwards is good for finding suffixes. You should
    use same functions for both compilation and match, since otherwise you
    must to provide pattern and string reversed to against each other.
*/

// Simple matching automata functions
BOOL automata_ascii_backwards_match(char *automata, char *string);
BOOL automata_ascii_forwards_match(char *automata, char *string);
// Annotated automata functions
size_t automata_ascii_backwards_select(char *automata, char *string);
size_t automata_ascii_forwards_select(char *automata, char *string);

// Create temporary structure for automata compilation
// If `annotated` is FALSE, `result` argument in following functions is unused
AUTOMATA automata_ascii_new(BOOL annotated);
void automata_ascii_free(AUTOMATA automata);
// Allocates final object with allocator function in case you want to keep it
// in some pool or anything. Temporary data is always allocated with malloc
// WARNING: Calling compile multiple times is not supported
char *automata_ascii_compile(AUTOMATA automata, size_t *size,
    void *(*allocator)(size_t size));
// Exact matching automata functions. Returns FALSE if same pattern is already
// matched by this automata
BOOL automata_ascii_add_forwards(AUTOMATA automata, char *str, size_t result);
BOOL automata_ascii_add_backwards(AUTOMATA automata, char *str, size_t result);
// Pattern matching counterparts. Currently recognizes only '*' for any number
// of any characters (including none). No backtracking, so mostly useful for
// prefix matching
BOOL automata_ascii_add_forwards_star(AUTOMATA automata, char *str, size_t result);
BOOL automata_ascii_add_backwards_star(AUTOMATA automata, char *str, size_t result);

#endif
