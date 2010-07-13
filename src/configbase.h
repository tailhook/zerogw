#ifndef _H_CONFIGBASE
#define _H_CONFIGBASE

#include <obstack.h>

#define FALSE   0
#define TRUE    1
#define bool    int
#define CARRAY_LOOP(type, varname, value) \
    for(type varname=value; varname; varname = (type)varname->head.next)

struct memcached_info {
    char **hostnames;
    char **unix_sockets;
};

typedef struct config_head_s {
    struct obstack pieces;
    void **freelist;
} config_head_t;

typedef struct mapping_element_s {
    struct mapping_element_s *next;
    char *key; int key_len;
} mapping_element_t;

typedef struct array_element_s {
    struct array_element_s *next;
} array_element_t;

typedef void (*config_defaults_func_t)(void *);
typedef void (*read_options_func_t)(int argc, char**argv, void *);
typedef struct config_meta_s {
    config_defaults_func_t config_defaults;
    read_options_func_t read_options;
    char *service_name;
    struct state_info_s *config_states;
} config_meta_t;

extern config_meta_t config_meta;

void prepare_configuration(int argc, char **argv);

#endif //_H_CONFIG
