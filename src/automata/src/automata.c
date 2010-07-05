#define __AUTOMATA_INTERNALS
#define AUTOMATA struct automata *
#define STATE struct state
#include "automata.h"
#include <string.h>
#include <assert.h>
#include <alloca.h>

struct automata {
    BOOL annotated;
    int states;
    STATE *first_state;
};

struct state {
    BOOL final;
    STATE *transitions[128];
    size_t result;
    int transition_count;
    int back_position;
    int offset; // If multiple references are implemented, that this is to be in
                // reference structure
};

/* Compiled automata format:
Data starts with sequence: "NFA" or "AFA" (Nondeterministic finite automata,
Annotated finite automata), where latter means that matching can yield a
result. Then sequence of states is specified starting with start state, and
otherwise in no particular order. When writing, order should be optimized
to minimize size of binary data and increase cache locality.

Each state starts with byte, which specifies number of transitions (1-127).
The most significant bit of it specifies if this state is final, which means
if matching ends on this state, matching will be successfull. If automata
started with "AFA" then after this byte there is result in the form of VLI.
Each transition starts with byte, which if '\0' means any symbol, and if not
nul, means symbol on which transition should occur. Following is VLI.
If it is zero state is unchanged, otherwise it is offset between current
position (starting of the VLI) and target state.

VLI is Variable Length Integer. It's 128-base integer which must be read
byte by byte, with most significant bit if set means you should continue
to read. It's written in most significant byte first order.
*/

#define READVLI(p, r) \
    { \
        r = 0; \
        unsigned char c = 0; \
        do { \
            c = *(p++); \
            r <<= 7; \
            r |= (c & ~128); \
        } while(c & 128); \
    }
#define SKIPVLI(p) \
    while(*p++ & 128);

BOOL automata_ascii_backwards_match(char *m, char *s) {
    assert((m[0] == 'N' || m[0] == 'A') && m[1] == 'F' && m[2] == 'A');
    BOOL annotated = *m == 'A';
    m += 3;
    char *sb = s;
    while(*s) ++s;
    for(;;) {
        --s;
        char *oldm = m;
        int cnt = *m & 127;
        BOOL last = *((unsigned char *)m++) & 128;
        if(s < sb) {
            return last;
        }
        if(annotated && last) {
            SKIPVLI(m);
        }
        int i;
        for(i = 0 ;i < cnt; ++i) {
            if(!*m || *m == *s) {
                if(*++m) {
                    char *p = m;
                    int o;
                    READVLI(p, o);
                    m = m + o;
                } else {
                    m = oldm;
                }
                break;
            }
            ++m;
            SKIPVLI(m);
        }
        if(i == cnt) {
            return FALSE;
        }
    }
}

size_t automata_ascii_backwards_select(char *m, char *s) {
    assert(m[0] == 'A' && m[1] == 'F' && m[2] == 'A');
    m += 3;
    char *sb = s;
    while(*s) ++s;
    for(;;) {
        --s;
        char *oldm = m;
        int cnt = *m & 127;
        BOOL last = *((unsigned char *)m++) & 128;
        size_t res;
        if(last) {
            READVLI(m, res);
        }
        if(s < sb) {
            return last ? res : 0;
        }
        int i;
        for(i = 0 ;i < cnt; ++i) {
            if(!*m || *m == *s) {
                if(*++m) {
                    char *p = m;
                    int o;
                    READVLI(p, o);
                    m = m + o;
                } else {
                    m = oldm;
                }
                break;
            }
            ++m;
            SKIPVLI(m);
        }
        if(i == cnt) {
            return FALSE;
        }
    }
}

BOOL automata_ascii_forwards_match(char *m, char *s) {
    assert((m[0] == 'N' || m[0] == 'A') && m[1] == 'F' && m[2] == 'A');
    BOOL annotated = *m == 'A';
    m += 3;
    for(;;) {
        char *oldm = m;
        int cnt = *m & 127;
        BOOL last = *((unsigned char *)m++) & 128;
        if(!*s) {
            return last;
        }
        if(annotated && last) {
            SKIPVLI(m);
        }
        int i;
        for(i = 0 ;i < cnt; ++i) {
            if(!*m || *m == *s) {
                if(*++m) {
                    char *p = m;
                    int o;
                    READVLI(p, o);
                    m = m + o;
                } else {
                    m = oldm;
                }
                break;
            }
            ++m;
            SKIPVLI(m);
        }
        if(i == cnt) {
            return FALSE;
        }
        ++s;
    }
}

size_t automata_ascii_forwards_select(char *m, char *s) {
    assert(m[0] == 'A' && m[1] == 'F' && m[2] == 'A');
    m += 3;
    for(;;) {
        char *oldm = m;
        int cnt = *m & 127;
        BOOL last = *((unsigned char *)m++) & 128;
        size_t res;
        if(last) {
            READVLI(m, res);
        }
        if(!*s) {
            return last ? res : 0;
        }
        int i;
        for(i = 0 ;i < cnt; ++i) {
            if(!*m || *m == *s) {
                if(*++m) {
                    char *p = m;
                    int o;
                    READVLI(p, o);
                    m = m + o;
                } else {
                    m = oldm;
                }
                break;
            }
            ++m;
            SKIPVLI(m);
        }
        if(i == cnt) {
            return FALSE;
        }
        ++s;
    }
}

STATE *create_state(AUTOMATA m) {
    STATE *result = malloc(sizeof(STATE));
    memset(result, 0, sizeof(STATE));
    ++ m->states;
    return result;
}

AUTOMATA automata_ascii_new(BOOL annotated) {
    AUTOMATA result = malloc(sizeof(struct automata));
    result->annotated = !!annotated;
    result->states = 0;
    result->first_state = create_state(result);
    return result;
}

void free_state(STATE *s) {
    for(int i = 0; i < 128; ++i) {
        STATE *c = s->transitions[i];
        if(c && c != s) {
            free_state(c);
        }
    }
    free(s);
}

void automata_ascii_free(AUTOMATA automata) {
    free_state(automata->first_state);
    free(automata);
}
char *automata_ascii_compile(AUTOMATA automata, size_t *size,
    void *(*allocator)(size_t size))
{
    STATE *stack[automata->states];
    STATE *states[automata->states];
    int stack_pos = 0;
    int states_pos = 0;
    stack[stack_pos++] = automata->first_state;
    while(stack_pos) {
        STATE *cur = stack[--stack_pos];
        states[states_pos++] = cur;
        for(int i = 0; i < 128; ++i) {
            STATE *t = cur->transitions[i];
            if(t && t != cur) {
                stack[stack_pos++] = t;
            }
        }
    }
    assert(states_pos == automata->states);
    int backbuf_pos = 0;
    while(states_pos-- > 0) {
        STATE *cur = states[states_pos];
        for(int i = 0; i < 128; ++i) {
            STATE *t = cur->transitions[i];
            if(!t) continue;
            if(t == cur) {
                ++backbuf_pos;
            } else if(backbuf_pos - t->back_position  < 128) {
                t->offset = backbuf_pos - t->back_position;
                ++backbuf_pos;
            } else if(backbuf_pos+1 - t->back_position  < 16384) {
                t->offset = backbuf_pos+1 - t->back_position;
                backbuf_pos += 2;
            } else if(backbuf_pos+2 - t->back_position < 2048383) {
                t->offset = backbuf_pos+2 - t->back_position;
                backbuf_pos += 3;
            } else {
                t->offset = backbuf_pos+3 - t->back_position;
                assert(backbuf_pos+3 - t->back_position < 260144641);
                backbuf_pos += 4;
            }
            ++backbuf_pos; // character (i)
        }
        if(automata->annotated && cur->final) {
            size_t v = cur->result;
            do {
                ++backbuf_pos;
                v >>= 7;
            } while(v);
        }
        cur->back_position = backbuf_pos++; // size byte
    }
    backbuf_pos += 3; // NFA | AFA

    if(size) *size = backbuf_pos;
    if(!allocator) allocator = malloc;
    char *result = allocator(backbuf_pos);
    char *buf = result;
    *buf++ = automata->annotated ? 'A' : 'N';
    *buf++ = 'F';
    *buf++ = 'A';
    for(int i = 0; i < automata->states; ++i) {
        STATE *cur = states[i];
        *buf++ = (cur->transition_count)|(cur->final ? 128 : 0);
        if(automata->annotated && cur->final) {
            unsigned char vbuf[10];
            int bytes = 0;
            size_t v = cur->result;
            do {
                vbuf[bytes++] = (v & 127)|128;
                v >>= 7;
            } while(v);
            vbuf[0] &= ~128;
            for(int k = 0; k < bytes; ++k) {
                buf[k] = vbuf[bytes-k-1];
            }
            buf += bytes;
        }
        for(int j = 127; j >= 0; --j) {
            STATE *t = cur->transitions[j];
            if(!t) continue;
            *buf++ = j;
            if(t == cur) {
                *buf++ = 0;
            } else {
                unsigned char vbuf[10];
                int bytes = 0;
                size_t v = t->offset;
                do {
                    vbuf[bytes++] = (v & 127)|128;
                    v >>= 7;
                } while(v);
                vbuf[0] &= ~128;
                for(int k = 0; k < bytes; ++k) {
                    buf[k] = vbuf[bytes-k-1];
                }
                buf += bytes;
            }
        }
    }
    assert(buf - result == backbuf_pos);
    return result;
}

BOOL automata_ascii_add_forwards(AUTOMATA automata, char *str, size_t result) {
    STATE *s = automata->first_state;
    char *p = str;
    if(!*p) {
        s->final = TRUE;
        s->result = result;
        return TRUE;
    }
    do {
        unsigned char c = *p;
        assert(c < 128);
        STATE *new_s = s->transitions[c];
        if(!new_s) {
            new_s = create_state(automata);
            s->transitions[c] = new_s;
            ++s->transition_count;
        }
        s = new_s;
    } while(*++p);
    if(s->final) {
        return FALSE;
    }
    s->final = TRUE;
    s->result = result;
    return TRUE;
}

BOOL automata_ascii_add_backwards(AUTOMATA automata, char *str, size_t result) {
    STATE *s = automata->first_state;
    char *p = str;
    if(!*p) {
        s->final = TRUE;
        s->result = result;
        return TRUE;
    }
    while(*p) ++p;
    do {
        --p;
        unsigned char c = *p;
        assert(c < 128);
        STATE *new_s = s->transitions[c];
        if(!new_s) {
            new_s = create_state(automata);
            s->transitions[c] = new_s;
            ++s->transition_count;
        }
        s = new_s;
    } while(p != str);
    if(s->final) {
        return FALSE;
    }
    s->final = TRUE;
    s->result = result;
    return TRUE;
}

BOOL automata_ascii_add_forwards_star(AUTOMATA automata, char *str, size_t result) {
    STATE *s = automata->first_state;
    char *p = str;
    if(!*p) {
        s->final = TRUE;
        s->result = result;
        return TRUE;
    }
    do {
        unsigned char c = *p;
        if(c == '*') c = 0;
        assert(c < 128);
        STATE *new_s = s->transitions[c];
        if(!new_s) {
            new_s = create_state(automata);
            s->transitions[c] = new_s;
            ++s->transition_count;
        }
        if(!c) { // star is magic
            // adds cyclic reference for star
            new_s->transitions[0] = new_s;
            ++new_s->transition_count;
        }
        s = new_s;
    } while(*++p);
    if(s->final) {
        return FALSE;
    }
    s->final = TRUE;
    s->result = result;
    return TRUE;
}

BOOL automata_ascii_add_backwards_star(AUTOMATA automata, char *str, size_t result) {
    STATE *s = automata->first_state;
    char *p = str;
    if(!*p) {
        s->final = TRUE;
        s->result = result;
        return TRUE;
    }
    while(*p) ++p;
    do {
        --p;
        unsigned char c = *p;
        if(c == '*') c = 0;
        assert(c < 128);
        STATE *new_s = s->transitions[c];
        if(!new_s) {
            new_s = create_state(automata);
            s->transitions[c] = new_s;
            ++s->transition_count;
        }
        if(!c) { // star is magic
            // adds cyclic reference for star
            new_s->transitions[0] = new_s;
            ++new_s->transition_count;
        }
        s = new_s;
    } while(p != str);
    if(s->final) {
        return FALSE;
    }
    s->final = TRUE;
    s->result = result;
    return TRUE;
}
