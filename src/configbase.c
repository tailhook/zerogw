#include <stddef.h>
#include <obstack.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <yaml.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#define STATE_FUN(name) int name(config_parsing_info_t *info, \
    struct state_info_s *state)
#ifdef CONFIG_DEBUG
#define STATE_BEGIN(name) LDEBUG("STATE: %d (%s)", state->id, #name);
#else
#define STATE_BEGIN(name)
#endif

#define SYNTAX_ERROR(cond) ACRIT2(cond, \
    "Syntax error in config file ``%s'' at line %d column %d (event %d)", \
    config_filename, info->event.start_mark.line, \
    info->event.start_mark.column, info->event.type)
#define VALUE_ERROR(cond, message, ...) ACRIT2(cond, \
    "Error at %s:%d[%d]: " message, config_filename, \
    info->event.start_mark.line, info->event.start_mark.column, ##__VA_ARGS__)
#define RESULT(type) *(type*)(((char *)info->config)+state->offset)
#define RESULT2(type) *(type*)(((char *)oldconfig)+state->offset)
#define RESULTLEN(type) \
    *(int*)(((char *)info->config)+state->offset + sizeof(type))
#define SCALARDUP(info) cfgdup(info->head, info->event.data.scalar.value, \
    info->event.data.scalar.length)


#define ANCHOR_ITEMS_MAX 64
#define ANCHOR_EVENTS_MAX 1024
typedef struct config_parsing_info_s {
    struct state_info_s *states;
    char *section_name;
    void *config;
    struct config_head_s *head;
    yaml_parser_t parser;
    yaml_event_t event;
    int anchor_level;
    int anchor_pos;
    int anchor_count;
    int anchor_event_count;
    yaml_event_t anchor_events[ANCHOR_EVENTS_MAX];
    struct {
        char name[64];
        int begin;
    } anchors[ANCHOR_ITEMS_MAX];
} config_parsing_info_t;

typedef STATE_FUN((*state_func_t));
typedef void (*set_defaults_fun_t)(void *cfg, void *el);

typedef struct struct_transition_s {
    char *symbol;
    int destination;
} struct_transition_t;

typedef enum  {
    CONFIG_INT,
    CONFIG_UINT,
    CONFIG_STRUCT,
    CONFIG_BOOL,
    CONFIG_FLOAT,
    CONFIG_USER,
    CONFIG_GROUP,
    CONFIG_HEX,
    CONFIG_ARRAY,
    CONFIG_MAPPING,
    CONFIG_LISTEN,
    CONFIG_SERVICE,
    CONFIG_FILE,
    CONFIG_DIR,
    CONFIG_STRING,
    CONFIG_ENTRY_TYPES
} config_entry_t;

typedef enum {
    LISTENING_MODEL_SINGLE,
    LISTENING_MODEL_MEMCACHED,
    LISTENING_MODELS
} config_listening_model_t;

typedef struct state_info_s {
    int id;
    int return_state;
    state_func_t func;
    int offset;
    config_entry_t entry_type;
    union {
        struct {
            int bitmask;
            int default_;
            int min;
            int max;
        } o_int;
        struct {
            int bitmask;
            unsigned int default_;
            unsigned int min;
            unsigned int max;
        } o_uint;
        struct {
            struct_transition_t *transitions;
        } o_struct;
        struct {
            int bitmask;
            bool default_;
        } o_bool;
        struct {
            int bitmask;
            double default_;
            double min;
            double max;
        } o_float;
        struct {
            char *default_;
        } o_user;
        struct {
            char *default_;
        } o_group;
        struct {
            size_t length;
        } o_hex;
        struct {
            int inner_state;
            struct array_element_s *current_element;
            int element_size;
            set_defaults_fun_t defaults_fun;
        } o_array;
        struct {
            int inner_state;
            struct mapping_element_s *current_element;
            int element_size;
            set_defaults_fun_t defaults_fun;
        } o_mapping;
        struct {
            config_listening_model_t model;
            struct sockaddr *default_; int default__len;
        } o_listen;
        struct {
            config_listening_model_t model;
            struct sockaddr *default_; int default__len;
        } o_service;
        struct {
            char *default_; int default__len;
            bool check_existence;
            bool check_dir;
            char *warn_outside;
        } o_file;
        struct {
            char *default_; int default__len;
            bool check_existence;
            bool check_dir;
        } o_dir;
        struct {
            char *default_; int default__len;
        } o_string;
    } options;
} state_info_t;


STATE_FUN(config_state_int);
STATE_FUN(config_state_uint);
STATE_FUN(config_state_struct);
STATE_FUN(config_state_bool);
STATE_FUN(config_state_float);
STATE_FUN(config_state_user);
STATE_FUN(config_state_group);
STATE_FUN(config_state_hex);
STATE_FUN(config_state_array);
STATE_FUN(config_state_mapping);
STATE_FUN(config_state_listen);
STATE_FUN(config_state_service);
STATE_FUN(config_state_file);
STATE_FUN(config_state_dir);
STATE_FUN(config_state_string);

char *config_filename = "/etc/counter.yaml";

#define IN_CONFIG_C
#include "config.h"
#include "log.h"

config_t config;

void init_config(void *config);
void read_config_generic(config_meta_t *meta, void *config);

char *cfgdup(config_head_t *head, char *str, int length) {
    char *res=obstack_alloc(&head->pieces, length+1);
    strcpy(res, str);
    return res;
}

void prepare_configuration(int argc, char **argv) {
    loglevel = LOG_NOTICE;
    config_meta.read_options(argc, argv, NULL);
    init_config(&config);
    config_meta.config_defaults(&config);
    read_config_generic(&config_meta, &config);
    config_meta.read_options(argc, argv, &config);
    loglevel = config.Globals.logging.level;
}

static void advance_yaml_parser(config_parsing_info_t *info) {
    if(info->anchor_pos >= 0) {
        memcpy(&info->event, &info->anchor_events[info->anchor_pos++],
            sizeof(info->event));
        switch(info->event.type) {
            case YAML_ALIAS_EVENT:
                LNIMPL("Nested aliases not supported");
                break;
            case YAML_MAPPING_START_EVENT:
            case YAML_SEQUENCE_START_EVENT:
                if(info->anchor_level >= 0) {
                    ++ info->anchor_level;
                }
            case YAML_SCALAR_EVENT:
                break;
            case YAML_MAPPING_END_EVENT:
            case YAML_SEQUENCE_END_EVENT:
                if(info->anchor_level >= 0) {
                    -- info->anchor_level;
                }
                break;
            default:
                ANIMPL(info->event.type);
                break;
        }
        if(info->anchor_level == 0) {
            info->anchor_pos = -1;
        }
        return;
    }
    if(info->event.type && info->anchor_level < 0) {
        yaml_event_delete(&info->event);
    }
    if(info->anchor_level == 0) {
        info->anchor_level = -1;
    }
    AERR(yaml_parser_parse(&info->parser, &info->event));
    switch(info->event.type) {
        case YAML_ALIAS_EVENT:
            for(int i = 0; i < info->anchor_count; ++i) {
                if(!strcmp(info->event.data.alias.anchor,
                    info->anchors[i].name)) {
                    info->anchor_pos = info->anchors[i].begin;
                    info->anchor_level = 0;
                    return advance_yaml_parser(info);
                }
            }
            LNIMPL("Anchor not found");
            break;
        case YAML_MAPPING_START_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            if(info->anchor_level >= 0) {
                ++ info->anchor_level;
            }
        case YAML_SCALAR_EVENT:
            if(info->event.data.scalar.anchor) {
                ANIMPL(info->anchor_count < 64);
                strncpy(info->anchors[info->anchor_count].name, info->event.data.scalar.anchor, 64);
                info->anchors[info->anchor_count].name[63] = 0;
                info->anchors[info->anchor_count].begin = info->anchor_event_count;
                ++ info->anchor_count;
                if(info->anchor_level < 0) { // Supporting nested aliases
                    info->anchor_level = 0;
                }
            }
            break;
        case YAML_MAPPING_END_EVENT:
        case YAML_SEQUENCE_END_EVENT:
            if(info->anchor_level >= 0) {
                -- info->anchor_level;
            }
            break;
        case YAML_STREAM_START_EVENT:
        case YAML_STREAM_END_EVENT:
        case YAML_DOCUMENT_START_EVENT:
        case YAML_DOCUMENT_END_EVENT:
            ANIMPL(info->anchor_level < 0);
            break;
        default:
            SYNTAX_ERROR(0);
            break;
    }
    if(info->anchor_level >= 0) {
        memcpy(&info->anchor_events[info->anchor_event_count++],
            &info->event, sizeof(info->event));
        if(!info->anchor_level) {
            info->anchor_level = -1;
        }
    }
#ifdef CONFIG_DEBUG
    if(info->event.type == YAML_SCALAR_EVENT) {
        LDEBUG("EVENT: %d (%s)", info->event.type, info->event.data.scalar.value);
    } else {
        LDEBUG("EVENT: %d", info->event.type);
    }
#endif // CONFIG_DEBUG
}

static void skip_subtree(config_parsing_info_t *info) {
    int level = 0;
    do {
        advance_yaml_parser(info);
        switch(info->event.type) {
            case YAML_MAPPING_START_EVENT:
            case YAML_SEQUENCE_START_EVENT:
                ++ level;
                break;
            case YAML_MAPPING_END_EVENT:
            case YAML_SEQUENCE_END_EVENT:
                -- level;
                break;
        }
    } while(level);
}

static void parse_global_config(config_parsing_info_t *info) {
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_START_EVENT);
    int state = 0;
    for(;info->event.type != YAML_MAPPING_END_EVENT || state != 0;
        advance_yaml_parser(info)) {
        ANIMPL(state == info->states[state].id);
        state = info->states[state].func(info, &info->states[state]);
    }
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_END_EVENT);
}

static void parse_specific_config(config_parsing_info_t *info) {
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_START_EVENT);
    int state = 0;
    for(;info->event.type != YAML_MAPPING_END_EVENT || state != 0;
        advance_yaml_parser(info)) {
        ANIMPL(state == info->states[state].id);
        state = info->states[state].func(info, &info->states[state]);
    }
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_END_EVENT);
}

static void the_very_beginning(config_parsing_info_t *info) {
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_STREAM_START_EVENT);
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_DOCUMENT_START_EVENT);
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_START_EVENT);
    advance_yaml_parser(info);

    for(;info->event.type != YAML_MAPPING_END_EVENT;advance_yaml_parser(info)) {
        SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
        if(!strcmp(info->event.data.scalar.value, "Global")) {
            parse_global_config(info);
        } else if(!strcmp(info->event.data.scalar.value, info->section_name)) {
            parse_specific_config(info);
        } else {
            skip_subtree(info);
        }
    }

    SYNTAX_ERROR(info->event.type == YAML_MAPPING_END_EVENT);
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_DOCUMENT_END_EVENT);
    advance_yaml_parser(info);
    SYNTAX_ERROR(info->event.type == YAML_STREAM_END_EVENT);
}

void init_config(void *config) {
    config_head_t *head = config;
    obstack_init(&head->pieces);
    head->freelist = NULL;
}

void free_config(void *config) {
    config_head_t *head = config;
    obstack_free(&head->pieces, NULL);
    if(head->freelist) {
        for(void **f = head->freelist; *f; ++f) {
            free(*f);
        }
        free(head->freelist);
    }
}

void read_config_generic(config_meta_t *meta, void *config)
{
    config_parsing_info_t info;
    info.states = meta->config_states;
    info.section_name = meta->service_name;
    info.config = config;
    info.head = config;
    info.anchor_level = -1;
    info.anchor_pos = -1;
    info.anchor_count = 0;
    info.anchor_event_count = 0;
    info.event.type = YAML_NO_EVENT;

    FILE *file = fopen(config_filename, "r");
    ANIMPL2(file, "Can't open configuration file ``%s''", config_filename);
    yaml_parser_initialize(&info.parser);
    yaml_parser_set_input_file(&info.parser, file);

    the_very_beginning(&info);

    yaml_parser_delete(&info.parser);
    fclose(file);
}

STATE_FUN(config_state_int) {
    STATE_BEGIN(int);
    SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
    unsigned char *end;
    int val = strtol(info->event.data.scalar.value, (char **)&end, 0);
    SYNTAX_ERROR(end == info->event.data.scalar.value
        + info->event.data.scalar.length);
    VALUE_ERROR(!(state->options.o_int.bitmask&2) || val <= state->options.o_int.max,
        "Value must be less than or equal to %d", state->options.o_int.max);
    VALUE_ERROR(!(state->options.o_int.bitmask&1) || val >= state->options.o_int.min,
        "Value must be greater than or equal to %d", state->options.o_int.min);
    *(int *)(((char *)info->config)+state->offset) = val;
    return state->return_state;
}

STATE_FUN(config_state_uint) {
    STATE_BEGIN(uint);
    SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
    unsigned char *end;
    unsigned int val = strtol(info->event.data.scalar.value, (char **)&end, 0);
    SYNTAX_ERROR(end == info->event.data.scalar.value + info->event.data.scalar.length);
    VALUE_ERROR(!(state->options.o_uint.bitmask&4) || val <= state->options.o_uint.max,
        "Value must be less than or equal to %d", state->options.o_uint.max);
    VALUE_ERROR(!(state->options.o_uint.bitmask&2) || val >= state->options.o_uint.min,
        "Value must be greater than or equal to %d", state->options.o_uint.min);
    *(unsigned int *)(((char *)info->config)+state->offset) = val;
    return state->return_state;
}

STATE_FUN(config_state_string) {
    STATE_BEGIN(string);
    SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
    char *res = obstack_alloc(&info->head->pieces,
        info->event.data.scalar.length+1);
    strcpy(res, info->event.data.scalar.value);
    RESULT(char *) = res;
    RESULTLEN(char *) = info->event.data.scalar.length;
    return state->return_state;
}

STATE_FUN(config_state_struct) {
    STATE_BEGIN(struct);
    switch(info->event.type) {
        case YAML_MAPPING_START_EVENT:
            return state->id;
        case YAML_MAPPING_END_EVENT:
            return state->return_state;
        case YAML_SCALAR_EVENT:
            for(struct_transition_t *t = state->options.o_struct.transitions;
                t->symbol; ++t) {
                if(!strcmp(t->symbol, info->event.data.scalar.value)) {
                    return t->destination;
                }
            }
            skip_subtree(info);
            return state->id;
        default:
            SYNTAX_ERROR(0);
            break;
    }
}

STATE_FUN(config_state_bool) {
    STATE_BEGIN(bool);
    LNIMPL("config_state_bool");
}

STATE_FUN(config_state_float) {
    STATE_BEGIN(float);
    LNIMPL("config_state_float");
}

STATE_FUN(config_state_user) {
    STATE_BEGIN(user);
    struct passwd user;
    struct passwd *tmp;
    char buf[64];
    ANIMPL2(!getpwnam_r(info->event.data.scalar.value, &user, buf, 64, &tmp),
        "User ``%s'' is not found on the system",
        info->event.data.scalar.value);
    RESULT(uid_t) = user.pw_uid;
    return state->return_state;
}

STATE_FUN(config_state_group) {
    STATE_BEGIN(group);
    struct group group;
    struct group *tmp;
    char buf[64];
    ANIMPL2(!getgrnam_r(info->event.data.scalar.value, &group, buf, 64, &tmp),
        "Group ``%s'' is not found on the system",
        info->event.data.scalar.value);
    RESULT(gid_t) = group.gr_gid;
    return state->return_state;
}

STATE_FUN(config_state_hex) {
    STATE_BEGIN(hex);
    SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
    VALUE_ERROR(info->event.data.scalar.value[0] == (unsigned char)'0'
        && info->event.data.scalar.value[1] == (unsigned char)'x'
        && info->event.data.scalar.length ==
            state->options.o_hex.length*2 + 2,
        "\"0x\" and %d hex digits expected",
        state->options.o_hex.length*2);
    for(int i = state->options.o_hex.length-1; i >= 0; --i) {
        char buf[] = {0, 0, 0};
        buf[0] = info->event.data.scalar.value[2 + i*2];
        buf[1] = info->event.data.scalar.value[3 + i*2];
        ((char *)(((char *)info->config)+state->offset))[i] =
            strtol(buf, NULL, 16);
    }
    return state->return_state;
}

STATE_FUN(config_state_array) {
    STATE_BEGIN(array);
    SYNTAX_ERROR(info->event.type == YAML_SEQUENCE_START_EVENT);
    void *oldconfig = info->config;
    while(info->event.type!=YAML_SEQUENCE_END_EVENT) {
        array_element_t *el = obstack_alloc(&info->head->pieces,
            state->options.o_array.element_size);
        memset(el, 0, state->options.o_array.element_size);
        info->config = el;
        if(state->options.o_array.defaults_fun) {
            state->options.o_array.defaults_fun(oldconfig, el);
        }

        int child = state->options.o_array.inner_state;
        while(child != state->id) {
            advance_yaml_parser(info);
            if(info->event.type == YAML_SEQUENCE_END_EVENT) break;
            ANIMPL(child == info->states[child].id);
            child = info->states[child].func(info, &info->states[child]);
        }

        if(state->options.o_array.current_element) {
            state->options.o_array.current_element->next = el;
        }
        state->options.o_array.current_element = el;
        if(!RESULT2(array_element_t *)) {
            RESULT2(array_element_t *) = el;
        }
    }
    info->config = oldconfig;

    state->options.o_array.current_element = NULL;
    SYNTAX_ERROR(info->event.type == YAML_SEQUENCE_END_EVENT);
    return state->return_state;
}

STATE_FUN(config_state_mapping) {
    STATE_BEGIN(mapping);
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_START_EVENT);
    advance_yaml_parser(info);
    void *oldconfig = info->config;
    for(;info->event.type!=YAML_MAPPING_END_EVENT; advance_yaml_parser(info)) {
        SYNTAX_ERROR(info->event.type == YAML_SCALAR_EVENT);
        mapping_element_t *el = obstack_alloc(&info->head->pieces,
            state->options.o_mapping.element_size);
        memset(el, 0, state->options.o_mapping.element_size);
        el->key = SCALARDUP(info);
        el->key_len = info->event.data.scalar.length;
        if(state->options.o_mapping.defaults_fun) {
            state->options.o_mapping.defaults_fun(oldconfig, el);
        }
        info->config = el;

        int child = state->options.o_mapping.inner_state;
        while(child != state->id) {
            advance_yaml_parser(info);
            SYNTAX_ERROR(child == info->states[child].id);
            child = info->states[child].func(info, &info->states[child]);
        }

        if(state->options.o_mapping.current_element) {
            state->options.o_mapping.current_element->next = el;
        }
        state->options.o_mapping.current_element = el;
        if(!RESULT2(mapping_element_t *)) {
            RESULT2(mapping_element_t *) = el;
        }
    }
    info->config = oldconfig;
    state->options.o_mapping.current_element = NULL;
    SYNTAX_ERROR(info->event.type == YAML_MAPPING_END_EVENT);
    return state->return_state;
}

STATE_FUN(config_state_listen) {
    STATE_BEGIN(listen);
    switch(info->event.type) {
        case YAML_SEQUENCE_START_EVENT:
        case YAML_SEQUENCE_END_EVENT:
            VALUE_ERROR(0, "Listening several sockets is not implemented yet");
            break;
        case YAML_SCALAR_EVENT:
            if(!strcmp(info->event.data.scalar.tag, "!Tcp")) {
                char *t = strchr(info->event.data.scalar.value, ':');
                if(t) *t = 0;
                int len = sizeof(struct sockaddr_in);
                struct sockaddr_in *insock = (struct sockaddr_in *)\
                    obstack_alloc(&info->head->pieces, len);
                insock->sin_family = AF_INET;
                SYNTAX_ERROR(inet_aton(info->event.data.scalar.value,
                    &insock->sin_addr));
                if(t) {
                    unsigned char *end;
                    int port = strtol(t+1, (char **)&end, 0);
                    SYNTAX_ERROR(end == info->event.data.scalar.value
                        + info->event.data.scalar.length);
                    SYNTAX_ERROR(port < 65536 && port > 0)
                    insock->sin_port = htons(port);
                } else {
                    struct sockaddr_in *def = (struct sockaddr_in *)\
                        state->options.o_listen.default_;
                    VALUE_ERROR(def && def->sin_family == AF_INET,
                        "Must specify tcp port");
                    insock->sin_port = def->sin_port;
                }
                RESULT(struct sockaddr_in *) = insock;
                RESULTLEN(struct sockaddr_in *) = len;
            } else {
                int len = sizeof(sa_family_t)+info->event.data.scalar.length+1;
                struct sockaddr_un *unsock = (struct sockaddr_un *)\
                    obstack_alloc(&info->head->pieces, len);
                unsock->sun_family = AF_UNIX;
                strcpy(unsock->sun_path, info->event.data.scalar.value);
                RESULT(struct sockaddr_un *) = unsock;
                RESULTLEN(struct sockaddr_un *) = len;
            }
            return state->return_state;
        default:
            SYNTAX_ERROR(0);
            break;
    }
}

STATE_FUN(config_state_service) {
    STATE_BEGIN(service);
    LNIMPL("config_state_service");
}

STATE_FUN(config_state_file) {
    STATE_BEGIN(file);
    // check_exists
    // check_dir
    char *res = obstack_alloc(&info->head->pieces,
        info->event.data.scalar.length);
    strcpy(res, info->event.data.scalar.value);
    RESULT(char *) = res;
    RESULTLEN(char *) = info->event.data.scalar.length;
    return state->return_state;
}

STATE_FUN(config_state_dir) {
    STATE_BEGIN(dir);
    // check_exists
    // check_dir
    char *res = obstack_alloc(&info->head->pieces,
        info->event.data.scalar.length);
    strcpy(res, info->event.data.scalar.value);
    RESULT(char *) = res;
    RESULTLEN(char *) = info->event.data.scalar.length;
    return state->return_state;
}

