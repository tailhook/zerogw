
/* WARNING: THIS IS AUTOMATICALLY GENERATED FILE, DO NOT EDIT! */#define _GNU_SOURCE
#define _BSD_SOURCE
#define IN_CONFIG_C
#include <unistd.h>

#include "config.h"
#include "log.h"

#include "configbase.c"

static char *optstr_zerogw = "hc:l:w:e:";
static struct option options_zerogw[] = {
    { "help", FALSE, NULL, 1000 },
    { "config", TRUE, NULL, 1001 },
    { "access-log", TRUE, NULL, 1002 },
    { "log-level", TRUE, NULL, 1003 },
    { "warning-timeout", TRUE, NULL, 1004 },
    { "error-log", TRUE, NULL, 1005 },
    { NULL, 0, NULL, 0 } };

/*
    Each state has a pointer (offset) to where to store value. Each state has a
    type of the value. Including imaginary types like struct. Type is just
    a function which is called on event in this state. It usually just checks
    type and goes into inner or outer state (for mappings, sequences). Arrays
    and Mappings has slightly different meaning for offset, it's offset inside
    the element. Also stack of arrays/mappings and obstack for memory allocation
    are passed to state functions.
*/

typedef enum {
    CS_ZEROGW, //#0
    CS_ZEROGW_Globals, //#1
    CS_ZEROGW_PAGES, //#2
    CS_ZEROGW_Pages, //#3
    CS_ZEROGW_Server, //#4
    CS_ZEROGW_CONTENTS, //#5
    CS_ZEROGW_Globals_logging, //#6
    CS_ZEROGW_Globals_contents, //#7
    CS_ZEROGW_Globals_limits, //#8
    CS_ZEROGW_Globals_responses, //#9
    CS_ZEROGW_PAGES_2, //#10
    CS_ZEROGW_PAGES_responses, //#11
    CS_ZEROGW_PAGES_limits, //#12
    CS_ZEROGW_CONTENTS_2, //#13
    CS_ZEROGW_PAGES_logging, //#14
    CS_ZEROGW_PAGES_pages, //#15
    CS_ZEROGW_PAGES_contents, //#16
    CS_ZEROGW_LISTEN, //#17
    CS_ZEROGW_Server_listen, //#18
    CS_ZEROGW_Globals_logging_access, //#19
    CS_ZEROGW_Globals_logging_error, //#20
    CS_ZEROGW_Globals_logging_warning_timeout, //#21
    CS_ZEROGW_Globals_logging_level, //#22
    CS_ZEROGW_METHOD, //#23
    CS_ZEROGW_Globals_limits_max_body_size, //#24
    CS_ZEROGW_Globals_limits_method, //#25
    CS_ZEROGW_Globals_responses_uri_not_found, //#26
    CS_ZEROGW_Globals_responses_internal_error, //#27
    CS_ZEROGW_Globals_responses_domain_not_found, //#28
    CS_ZEROGW_PAGES_logging_2, //#29
    CS_ZEROGW_PAGES_responses_2, //#30
    CS_ZEROGW_PAGES_limits_2, //#31
    CS_ZEROGW_FORWARD, //#32
    CS_ZEROGW_CONTENTS_3, //#33
    CS_ZEROGW_PAGES_forward, //#34
    CS_ZEROGW_PAGES_path, //#35
    CS_ZEROGW_PAGES_contents_2, //#36
    CS_ZEROGW_PAGES_responses_uri_not_found, //#37
    CS_ZEROGW_PAGES_responses_internal_error, //#38
    CS_ZEROGW_PAGES_responses_domain_not_found, //#39
    CS_ZEROGW_METHOD_2, //#40
    CS_ZEROGW_PAGES_limits_max_body_size, //#41
    CS_ZEROGW_PAGES_limits_method, //#42
    CS_ZEROGW_PAGES_logging_access, //#43
    CS_ZEROGW_PAGES_logging_error, //#44
    CS_ZEROGW_PAGES_logging_warning_timeout, //#45
    CS_ZEROGW_PAGES_logging_level, //#46
    CS_ZEROGW_LISTEN_host, //#47
    CS_ZEROGW_LISTEN_port, //#48
    CS_ZEROGW_Globals_responses_uri_not_found_status, //#49
    CS_ZEROGW_Globals_responses_uri_not_found_body, //#50
    CS_ZEROGW_Globals_responses_uri_not_found_code, //#51
    CS_ZEROGW_Globals_responses_internal_error_status, //#52
    CS_ZEROGW_Globals_responses_internal_error_body, //#53
    CS_ZEROGW_Globals_responses_internal_error_code, //#54
    CS_ZEROGW_Globals_responses_domain_not_found_status, //#55
    CS_ZEROGW_Globals_responses_domain_not_found_body, //#56
    CS_ZEROGW_Globals_responses_domain_not_found_code, //#57
    CS_ZEROGW_PAGES_logging_access_2, //#58
    CS_ZEROGW_PAGES_logging_error_2, //#59
    CS_ZEROGW_PAGES_logging_warning_timeout_2, //#60
    CS_ZEROGW_PAGES_logging_level_2, //#61
    CS_ZEROGW_PAGES_responses_uri_not_found_2, //#62
    CS_ZEROGW_PAGES_responses_internal_error_2, //#63
    CS_ZEROGW_PAGES_responses_domain_not_found_2, //#64
    CS_ZEROGW_METHOD_3, //#65
    CS_ZEROGW_PAGES_limits_max_body_size_2, //#66
    CS_ZEROGW_PAGES_limits_method_2, //#67
    CS_ZEROGW_PAGES_responses_uri_not_found_status, //#68
    CS_ZEROGW_PAGES_responses_uri_not_found_body, //#69
    CS_ZEROGW_PAGES_responses_uri_not_found_code, //#70
    CS_ZEROGW_PAGES_responses_internal_error_status, //#71
    CS_ZEROGW_PAGES_responses_internal_error_body, //#72
    CS_ZEROGW_PAGES_responses_internal_error_code, //#73
    CS_ZEROGW_PAGES_responses_domain_not_found_status, //#74
    CS_ZEROGW_PAGES_responses_domain_not_found_body, //#75
    CS_ZEROGW_PAGES_responses_domain_not_found_code, //#76
    CS_ZEROGW_PAGES_responses_uri_not_found_status_2, //#77
    CS_ZEROGW_PAGES_responses_uri_not_found_body_2, //#78
    CS_ZEROGW_PAGES_responses_uri_not_found_code_2, //#79
    CS_ZEROGW_PAGES_responses_internal_error_status_2, //#80
    CS_ZEROGW_PAGES_responses_internal_error_body_2, //#81
    CS_ZEROGW_PAGES_responses_internal_error_code_2, //#82
    CS_ZEROGW_PAGES_responses_domain_not_found_status_2, //#83
    CS_ZEROGW_PAGES_responses_domain_not_found_body_2, //#84
    CS_ZEROGW_PAGES_responses_domain_not_found_code_2, //#85
    CS_ZEROGW_STATES_TOTAL,
} config_zerogw_state_t;


static struct_transition_t transitions_zerogw[] = {
    { "Globals", CS_ZEROGW_Globals }, 
    { "Pages", CS_ZEROGW_Pages }, 
    { "Server", CS_ZEROGW_Server }, 
    { NULL, 0 }, 
    { "logging", CS_ZEROGW_Globals_logging }, 
    { "contents", CS_ZEROGW_Globals_contents }, 
    { "limits", CS_ZEROGW_Globals_limits }, 
    { "responses", CS_ZEROGW_Globals_responses }, 
    { NULL, 0 }, 
    { "responses", CS_ZEROGW_PAGES_responses }, 
    { "limits", CS_ZEROGW_PAGES_limits }, 
    { "logging", CS_ZEROGW_PAGES_logging }, 
    { "pages", CS_ZEROGW_PAGES_pages }, 
    { "contents", CS_ZEROGW_PAGES_contents }, 
    { NULL, 0 }, 
    { "listen", CS_ZEROGW_Server_listen }, 
    { NULL, 0 }, 
    { "access", CS_ZEROGW_Globals_logging_access }, 
    { "error", CS_ZEROGW_Globals_logging_error }, 
    { "warning-timeout", CS_ZEROGW_Globals_logging_warning_timeout }, 
    { "level", CS_ZEROGW_Globals_logging_level }, 
    { NULL, 0 }, 
    { "max-body-size", CS_ZEROGW_Globals_limits_max_body_size }, 
    { "method", CS_ZEROGW_Globals_limits_method }, 
    { NULL, 0 }, 
    { "uri-not-found", CS_ZEROGW_Globals_responses_uri_not_found }, 
    { "internal-error", CS_ZEROGW_Globals_responses_internal_error }, 
    { "domain-not-found", CS_ZEROGW_Globals_responses_domain_not_found }, 
    { NULL, 0 }, 
    { "logging", CS_ZEROGW_PAGES_logging_2 }, 
    { "responses", CS_ZEROGW_PAGES_responses_2 }, 
    { "limits", CS_ZEROGW_PAGES_limits_2 }, 
    { "forward", CS_ZEROGW_PAGES_forward }, 
    { "path", CS_ZEROGW_PAGES_path }, 
    { "contents", CS_ZEROGW_PAGES_contents_2 }, 
    { NULL, 0 }, 
    { "uri-not-found", CS_ZEROGW_PAGES_responses_uri_not_found }, 
    { "internal-error", CS_ZEROGW_PAGES_responses_internal_error }, 
    { "domain-not-found", CS_ZEROGW_PAGES_responses_domain_not_found }, 
    { NULL, 0 }, 
    { "max-body-size", CS_ZEROGW_PAGES_limits_max_body_size }, 
    { "method", CS_ZEROGW_PAGES_limits_method }, 
    { NULL, 0 }, 
    { "access", CS_ZEROGW_PAGES_logging_access }, 
    { "error", CS_ZEROGW_PAGES_logging_error }, 
    { "warning-timeout", CS_ZEROGW_PAGES_logging_warning_timeout }, 
    { "level", CS_ZEROGW_PAGES_logging_level }, 
    { NULL, 0 }, 
    { "host", CS_ZEROGW_LISTEN_host }, 
    { "port", CS_ZEROGW_LISTEN_port }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_Globals_responses_uri_not_found_status }, 
    { "body", CS_ZEROGW_Globals_responses_uri_not_found_body }, 
    { "code", CS_ZEROGW_Globals_responses_uri_not_found_code }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_Globals_responses_internal_error_status }, 
    { "body", CS_ZEROGW_Globals_responses_internal_error_body }, 
    { "code", CS_ZEROGW_Globals_responses_internal_error_code }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_Globals_responses_domain_not_found_status }, 
    { "body", CS_ZEROGW_Globals_responses_domain_not_found_body }, 
    { "code", CS_ZEROGW_Globals_responses_domain_not_found_code }, 
    { NULL, 0 }, 
    { "access", CS_ZEROGW_PAGES_logging_access_2 }, 
    { "error", CS_ZEROGW_PAGES_logging_error_2 }, 
    { "warning-timeout", CS_ZEROGW_PAGES_logging_warning_timeout_2 }, 
    { "level", CS_ZEROGW_PAGES_logging_level_2 }, 
    { NULL, 0 }, 
    { "uri-not-found", CS_ZEROGW_PAGES_responses_uri_not_found_2 }, 
    { "internal-error", CS_ZEROGW_PAGES_responses_internal_error_2 }, 
    { "domain-not-found", CS_ZEROGW_PAGES_responses_domain_not_found_2 }, 
    { NULL, 0 }, 
    { "max-body-size", CS_ZEROGW_PAGES_limits_max_body_size_2 }, 
    { "method", CS_ZEROGW_PAGES_limits_method_2 }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_uri_not_found_status }, 
    { "body", CS_ZEROGW_PAGES_responses_uri_not_found_body }, 
    { "code", CS_ZEROGW_PAGES_responses_uri_not_found_code }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_internal_error_status }, 
    { "body", CS_ZEROGW_PAGES_responses_internal_error_body }, 
    { "code", CS_ZEROGW_PAGES_responses_internal_error_code }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_domain_not_found_status }, 
    { "body", CS_ZEROGW_PAGES_responses_domain_not_found_body }, 
    { "code", CS_ZEROGW_PAGES_responses_domain_not_found_code }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_uri_not_found_status_2 }, 
    { "body", CS_ZEROGW_PAGES_responses_uri_not_found_body_2 }, 
    { "code", CS_ZEROGW_PAGES_responses_uri_not_found_code_2 }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_internal_error_status_2 }, 
    { "body", CS_ZEROGW_PAGES_responses_internal_error_body_2 }, 
    { "code", CS_ZEROGW_PAGES_responses_internal_error_code_2 }, 
    { NULL, 0 }, 
    { "status", CS_ZEROGW_PAGES_responses_domain_not_found_status_2 }, 
    { "body", CS_ZEROGW_PAGES_responses_domain_not_found_body_2 }, 
    { "code", CS_ZEROGW_PAGES_responses_domain_not_found_code_2 }, 
    { NULL, 0 }, 
    };

static state_info_t states_zerogw[] = {
    { id: CS_ZEROGW, return_state: -1, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, head), options: { o_struct: { transitions: &transitions_zerogw[0] } } },
    { id: CS_ZEROGW_Globals, return_state: CS_ZEROGW, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals), options: { o_struct: { transitions: &transitions_zerogw[1] } } },
    { id: CS_ZEROGW_PAGES, return_state: CS_ZEROGW_Pages, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, head), options: { o_struct: { transitions: &transitions_zerogw[2] } } },
    { id: CS_ZEROGW_Pages, return_state: CS_ZEROGW, entry_type: CONFIG_MAPPING, func: config_state_mapping, offset: offsetof(config_zerogw_t, Pages), options: { o_mapping: { current_element: NULL, element_size: sizeof(config_zerogw_Pages_t), inner_state: CS_ZEROGW_PAGES } } },
    { id: CS_ZEROGW_Server, return_state: CS_ZEROGW, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Server), options: { o_struct: { transitions: &transitions_zerogw[3] } } },
    { id: CS_ZEROGW_CONTENTS, return_state: CS_ZEROGW_Globals_contents, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_contents_t, value) },
    { id: CS_ZEROGW_Globals_logging, return_state: CS_ZEROGW_Globals, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.logging), options: { o_struct: { transitions: &transitions_zerogw[4] } } },
    { id: CS_ZEROGW_Globals_contents, return_state: CS_ZEROGW_Globals, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_t, Globals.contents), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_contents_t), inner_state: CS_ZEROGW_CONTENTS } } },
    { id: CS_ZEROGW_Globals_limits, return_state: CS_ZEROGW_Globals, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.limits), options: { o_struct: { transitions: &transitions_zerogw[5] } } },
    { id: CS_ZEROGW_Globals_responses, return_state: CS_ZEROGW_Globals, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.responses), options: { o_struct: { transitions: &transitions_zerogw[6] } } },
    { id: CS_ZEROGW_PAGES_2, return_state: CS_ZEROGW_PAGES_pages, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, head), options: { o_struct: { transitions: &transitions_zerogw[7] } } },
    { id: CS_ZEROGW_PAGES_responses, return_state: CS_ZEROGW_PAGES, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, responses), options: { o_struct: { transitions: &transitions_zerogw[8] } } },
    { id: CS_ZEROGW_PAGES_limits, return_state: CS_ZEROGW_PAGES, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, limits), options: { o_struct: { transitions: &transitions_zerogw[9] } } },
    { id: CS_ZEROGW_CONTENTS_2, return_state: CS_ZEROGW_PAGES_contents, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_contents_t, value) },
    { id: CS_ZEROGW_PAGES_logging, return_state: CS_ZEROGW_PAGES, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, logging), options: { o_struct: { transitions: &transitions_zerogw[10] } } },
    { id: CS_ZEROGW_PAGES_pages, return_state: CS_ZEROGW_PAGES, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_Pages_t, pages), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_pages_t), inner_state: CS_ZEROGW_PAGES_2 } } },
    { id: CS_ZEROGW_PAGES_contents, return_state: CS_ZEROGW_PAGES, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_Pages_t, contents), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_contents_t), inner_state: CS_ZEROGW_CONTENTS_2 } } },
    { id: CS_ZEROGW_LISTEN, return_state: CS_ZEROGW_Server_listen, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_listen_t, head), options: { o_struct: { transitions: &transitions_zerogw[11] } } },
    { id: CS_ZEROGW_Server_listen, return_state: CS_ZEROGW_Server, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_t, Server.listen), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_listen_t), inner_state: CS_ZEROGW_LISTEN } } },
    { id: CS_ZEROGW_Globals_logging_access, return_state: CS_ZEROGW_Globals_logging, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_t, Globals.logging.access), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/access.log", default__len: 29 } } },
    { id: CS_ZEROGW_Globals_logging_error, return_state: CS_ZEROGW_Globals_logging, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_t, Globals.logging.error), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/error.log", default__len: 28 } } },
    { id: CS_ZEROGW_Globals_logging_warning_timeout, return_state: CS_ZEROGW_Globals_logging, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.logging.warning_timeout), options: { o_int: { bitmask: 6, default_: 300, max: 3600, min: 0 } } },
    { id: CS_ZEROGW_Globals_logging_level, return_state: CS_ZEROGW_Globals_logging, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.logging.level), options: { o_int: { bitmask: 6, default_: 3, max: 7, min: 0 } } },
    { id: CS_ZEROGW_METHOD, return_state: CS_ZEROGW_Globals_limits_method, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_method_t, value) },
    { id: CS_ZEROGW_Globals_limits_max_body_size, return_state: CS_ZEROGW_Globals_limits, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.limits.max_body_size), options: { o_int: { bitmask: 2, default_: 65536, min: 0 } } },
    { id: CS_ZEROGW_Globals_limits_method, return_state: CS_ZEROGW_Globals_limits, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_t, Globals.limits.method), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_method_t), inner_state: CS_ZEROGW_METHOD } } },
    { id: CS_ZEROGW_Globals_responses_uri_not_found, return_state: CS_ZEROGW_Globals_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.responses.uri_not_found), options: { o_struct: { transitions: &transitions_zerogw[12] } } },
    { id: CS_ZEROGW_Globals_responses_internal_error, return_state: CS_ZEROGW_Globals_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.responses.internal_error), options: { o_struct: { transitions: &transitions_zerogw[13] } } },
    { id: CS_ZEROGW_Globals_responses_domain_not_found, return_state: CS_ZEROGW_Globals_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_t, Globals.responses.domain_not_found), options: { o_struct: { transitions: &transitions_zerogw[14] } } },
    { id: CS_ZEROGW_PAGES_logging_2, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, logging), options: { o_struct: { transitions: &transitions_zerogw[15] } } },
    { id: CS_ZEROGW_PAGES_responses_2, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, responses), options: { o_struct: { transitions: &transitions_zerogw[16] } } },
    { id: CS_ZEROGW_PAGES_limits_2, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, limits), options: { o_struct: { transitions: &transitions_zerogw[17] } } },
    { id: CS_ZEROGW_FORWARD, return_state: CS_ZEROGW_PAGES_forward, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_forward_t, value) },
    { id: CS_ZEROGW_CONTENTS_3, return_state: CS_ZEROGW_PAGES_contents_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_contents_t, value) },
    { id: CS_ZEROGW_PAGES_forward, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_pages_t, forward), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_forward_t), inner_state: CS_ZEROGW_FORWARD } } },
    { id: CS_ZEROGW_PAGES_path, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, path) },
    { id: CS_ZEROGW_PAGES_contents_2, return_state: CS_ZEROGW_PAGES_2, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_pages_t, contents), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_contents_t), inner_state: CS_ZEROGW_CONTENTS_3 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found, return_state: CS_ZEROGW_PAGES_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, responses.uri_not_found), options: { o_struct: { transitions: &transitions_zerogw[18] } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error, return_state: CS_ZEROGW_PAGES_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, responses.internal_error), options: { o_struct: { transitions: &transitions_zerogw[19] } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found, return_state: CS_ZEROGW_PAGES_responses, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_Pages_t, responses.domain_not_found), options: { o_struct: { transitions: &transitions_zerogw[20] } } },
    { id: CS_ZEROGW_METHOD_2, return_state: CS_ZEROGW_PAGES_limits_method, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_method_t, value) },
    { id: CS_ZEROGW_PAGES_limits_max_body_size, return_state: CS_ZEROGW_PAGES_limits, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, limits.max_body_size), options: { o_int: { bitmask: 2, default_: 65536, min: 0 } } },
    { id: CS_ZEROGW_PAGES_limits_method, return_state: CS_ZEROGW_PAGES_limits, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_Pages_t, limits.method), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_method_t), inner_state: CS_ZEROGW_METHOD_2 } } },
    { id: CS_ZEROGW_PAGES_logging_access, return_state: CS_ZEROGW_PAGES_logging, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_Pages_t, logging.access), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/access.log", default__len: 29 } } },
    { id: CS_ZEROGW_PAGES_logging_error, return_state: CS_ZEROGW_PAGES_logging, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_Pages_t, logging.error), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/error.log", default__len: 28 } } },
    { id: CS_ZEROGW_PAGES_logging_warning_timeout, return_state: CS_ZEROGW_PAGES_logging, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, logging.warning_timeout), options: { o_int: { bitmask: 6, default_: 300, max: 3600, min: 0 } } },
    { id: CS_ZEROGW_PAGES_logging_level, return_state: CS_ZEROGW_PAGES_logging, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, logging.level), options: { o_int: { bitmask: 6, default_: 3, max: 7, min: 0 } } },
    { id: CS_ZEROGW_LISTEN_host, return_state: CS_ZEROGW_LISTEN, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_listen_t, host) },
    { id: CS_ZEROGW_LISTEN_port, return_state: CS_ZEROGW_LISTEN, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_listen_t, port), options: { o_int: { bitmask: 6, default_: 80, max: 65536, min: 1 } } },
    { id: CS_ZEROGW_Globals_responses_uri_not_found_status, return_state: CS_ZEROGW_Globals_responses_uri_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.uri_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_Globals_responses_uri_not_found_body, return_state: CS_ZEROGW_Globals_responses_uri_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.uri_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_Globals_responses_uri_not_found_code, return_state: CS_ZEROGW_Globals_responses_uri_not_found, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.responses.uri_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: CS_ZEROGW_Globals_responses_internal_error_status, return_state: CS_ZEROGW_Globals_responses_internal_error, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.internal_error.status), options: { o_string: { default_: "Internal Server Error", default__len: 21 } } },
    { id: CS_ZEROGW_Globals_responses_internal_error_body, return_state: CS_ZEROGW_Globals_responses_internal_error, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.internal_error.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>500 Internal Server Error</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>Internal Server Error</h1>\n"
    "    Sorry, you can retry again later\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 170 } } },
    { id: CS_ZEROGW_Globals_responses_internal_error_code, return_state: CS_ZEROGW_Globals_responses_internal_error, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.responses.internal_error.code), options: { o_int: { bitmask: 6, default_: 500, max: 999, min: 100 } } },
    { id: CS_ZEROGW_Globals_responses_domain_not_found_status, return_state: CS_ZEROGW_Globals_responses_domain_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.domain_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_Globals_responses_domain_not_found_body, return_state: CS_ZEROGW_Globals_responses_domain_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_t, Globals.responses.domain_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_Globals_responses_domain_not_found_code, return_state: CS_ZEROGW_Globals_responses_domain_not_found, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_t, Globals.responses.domain_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_logging_access_2, return_state: CS_ZEROGW_PAGES_logging_2, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_pages_t, logging.access), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/access.log", default__len: 29 } } },
    { id: CS_ZEROGW_PAGES_logging_error_2, return_state: CS_ZEROGW_PAGES_logging_2, entry_type: CONFIG_FILE, func: config_state_file, offset: offsetof(config_zerogw_pages_t, logging.error), options: { o_file: { check_dir: TRUE, check_existence: FALSE, default_: "/var/applog/zerogw/error.log", default__len: 28 } } },
    { id: CS_ZEROGW_PAGES_logging_warning_timeout_2, return_state: CS_ZEROGW_PAGES_logging_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, logging.warning_timeout), options: { o_int: { bitmask: 6, default_: 300, max: 3600, min: 0 } } },
    { id: CS_ZEROGW_PAGES_logging_level_2, return_state: CS_ZEROGW_PAGES_logging_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, logging.level), options: { o_int: { bitmask: 6, default_: 3, max: 7, min: 0 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_2, return_state: CS_ZEROGW_PAGES_responses_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, responses.uri_not_found), options: { o_struct: { transitions: &transitions_zerogw[21] } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_2, return_state: CS_ZEROGW_PAGES_responses_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, responses.internal_error), options: { o_struct: { transitions: &transitions_zerogw[22] } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_2, return_state: CS_ZEROGW_PAGES_responses_2, entry_type: CONFIG_STRUCT, func: config_state_struct, offset: offsetof(config_zerogw_pages_t, responses.domain_not_found), options: { o_struct: { transitions: &transitions_zerogw[23] } } },
    { id: CS_ZEROGW_METHOD_3, return_state: CS_ZEROGW_PAGES_limits_method_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_method_t, value) },
    { id: CS_ZEROGW_PAGES_limits_max_body_size_2, return_state: CS_ZEROGW_PAGES_limits_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, limits.max_body_size), options: { o_int: { bitmask: 2, default_: 65536, min: 0 } } },
    { id: CS_ZEROGW_PAGES_limits_method_2, return_state: CS_ZEROGW_PAGES_limits_2, entry_type: CONFIG_ARRAY, func: config_state_array, offset: offsetof(config_zerogw_pages_t, limits.method), options: { o_array: { current_element: NULL, element_size: sizeof(config_zerogw_method_t), inner_state: CS_ZEROGW_METHOD_3 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_status, return_state: CS_ZEROGW_PAGES_responses_uri_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.uri_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_body, return_state: CS_ZEROGW_PAGES_responses_uri_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.uri_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_code, return_state: CS_ZEROGW_PAGES_responses_uri_not_found, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, responses.uri_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_status, return_state: CS_ZEROGW_PAGES_responses_internal_error, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.internal_error.status), options: { o_string: { default_: "Internal Server Error", default__len: 21 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_body, return_state: CS_ZEROGW_PAGES_responses_internal_error, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.internal_error.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>500 Internal Server Error</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>Internal Server Error</h1>\n"
    "    Sorry, you can retry again later\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 170 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_code, return_state: CS_ZEROGW_PAGES_responses_internal_error, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, responses.internal_error.code), options: { o_int: { bitmask: 6, default_: 500, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_status, return_state: CS_ZEROGW_PAGES_responses_domain_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.domain_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_body, return_state: CS_ZEROGW_PAGES_responses_domain_not_found, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_Pages_t, responses.domain_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_code, return_state: CS_ZEROGW_PAGES_responses_domain_not_found, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_Pages_t, responses.domain_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_status_2, return_state: CS_ZEROGW_PAGES_responses_uri_not_found_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.uri_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_body_2, return_state: CS_ZEROGW_PAGES_responses_uri_not_found_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.uri_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_PAGES_responses_uri_not_found_code_2, return_state: CS_ZEROGW_PAGES_responses_uri_not_found_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, responses.uri_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_status_2, return_state: CS_ZEROGW_PAGES_responses_internal_error_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.internal_error.status), options: { o_string: { default_: "Internal Server Error", default__len: 21 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_body_2, return_state: CS_ZEROGW_PAGES_responses_internal_error_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.internal_error.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>500 Internal Server Error</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>Internal Server Error</h1>\n"
    "    Sorry, you can retry again later\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 170 } } },
    { id: CS_ZEROGW_PAGES_responses_internal_error_code_2, return_state: CS_ZEROGW_PAGES_responses_internal_error_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, responses.internal_error.code), options: { o_int: { bitmask: 6, default_: 500, max: 999, min: 100 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_status_2, return_state: CS_ZEROGW_PAGES_responses_domain_not_found_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.domain_not_found.status), options: { o_string: { default_: "Not Found", default__len: 9 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_body_2, return_state: CS_ZEROGW_PAGES_responses_domain_not_found_2, entry_type: CONFIG_STRING, func: config_state_string, offset: offsetof(config_zerogw_pages_t, responses.domain_not_found.body), options: { o_string: { default_: 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "", default__len: 113 } } },
    { id: CS_ZEROGW_PAGES_responses_domain_not_found_code_2, return_state: CS_ZEROGW_PAGES_responses_domain_not_found_2, entry_type: CONFIG_INT, func: config_state_int, offset: offsetof(config_zerogw_pages_t, responses.domain_not_found.code), options: { o_int: { bitmask: 6, default_: 404, max: 999, min: 100 } } },
    { id: -1 } };

static void config_defaults_zerogw(void *cfg) {
    config_zerogw_t *config = cfg;
    config->Globals.logging.access_len = 29; config->Globals.logging.access = "/var/applog/zerogw/access.log";
    config->Globals.logging.error_len = 28; config->Globals.logging.error = "/var/applog/zerogw/error.log";
    config->Globals.logging.warning_timeout = 300;
    config->Globals.logging.level = 3;
    config->Globals.limits.max_body_size = 65536;
    config->Globals.responses.uri_not_found.status = "Not Found"; config->Globals.responses.uri_not_found.status_len = 9;
    config->Globals.responses.uri_not_found.body_len = 113; config->Globals.responses.uri_not_found.body = 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "";
    config->Globals.responses.uri_not_found.code = 404;
    config->Globals.responses.internal_error.status_len = 21; config->Globals.responses.internal_error.status = "Internal Server Error";
    config->Globals.responses.internal_error.body_len = 170; config->Globals.responses.internal_error.body = 
    "<html>\n"
    "  <head>\n"
    "    <title>500 Internal Server Error</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>Internal Server Error</h1>\n"
    "    Sorry, you can retry again later\n"
    "  </body>\n"
    "</html>\n"
    "";
    config->Globals.responses.internal_error.code = 500;
    config->Globals.responses.domain_not_found.status = "Not Found"; config->Globals.responses.domain_not_found.status_len = 9;
    config->Globals.responses.domain_not_found.body_len = 113; config->Globals.responses.domain_not_found.body = 
    "<html>\n"
    "  <head>\n"
    "    <title>404 Not Found</title>\n"
    "  </head>\n"
    "  <body>\n"
    "    <h1>404 Not Found</h1>\n"
    "  </body>\n"
    "</html>\n"
    "";
    config->Globals.responses.domain_not_found.code = 404;
}


void read_options_zerogw(int argc, char **argv, void *cfg) {
    config_zerogw_t *config = cfg;
    int o;
    optind = 1;
    while((o = getopt_long(argc, argv, optstr_zerogw,
        options_zerogw, NULL)) != -1) {
        switch(o) {

        case 1000:
        case 'h':
            printf(
                "Usage: makeconfig.py [options]\n"
                "\n"
                "Options:\n"
                "  -h, --help            show this help message and exit\n"
                "  -c FILE, --config=FILE\n"
                "                        Configuration file (default \"/etc/zerogw.yaml\")\n"
                "  -l GLOBALS.LOGGING.ACCESS, --access-log=GLOBALS.LOGGING.ACCESS\n"
                "                        Where to write access log messages. Use \"-\" for\n"
                "                        stderr.\n"
                "  --log-level=GLOBALS.LOGGING.LEVEL\n"
                "                        Verbosity of error log\n"
                "  -w GLOBALS.LOGGING.WARNING_TIMEOUT, --warning-timeout=GLOBALS.LOGGING.WARNING_TIMEOUT\n"
                "                        Timeout of displaying repeatable warnings Very small\n"
                "                        value will flood your logfile, but very big value will\n"
                "                        not make you confident when the problem is gone (wait\n"
                "                        at least this timeout and a minute or so on overloaded\n"
                "                        server, unless you know truth by some other means)\n"
                "  -e GLOBALS.LOGGING.ERROR, --error-log=GLOBALS.LOGGING.ERROR\n"
                "                        Where to write error log messages. Use \"-\" for stderr.\n"
                );
            exit(0);
            break;
        case 1001:
        case 'c':
            config_filename = optarg;
            break;
        case 1002:
        case 'l':
            if(config) {
                config->Globals.logging.access = optarg;
                config->Globals.logging.access_len = strlen(optarg);
            }
            break;
        case 1003:
            if(config) {
                config->Globals.logging.level = atoi(optarg); //TODO: check
            }
            break;
        case 1004:
        case 'w':
            if(config) {
                config->Globals.logging.warning_timeout = atoi(optarg); //TODO: check
            }
            break;
        case 1005:
        case 'e':
            if(config) {
                config->Globals.logging.error = optarg;
                config->Globals.logging.error_len = strlen(optarg);
            }
            break;
        default:
            fprintf(stderr, "Usage: %s [options]\n", argv[0]);
            exit(1);
        }
    }
}

#if defined(CONFIG_ZEROGW)
    config_defaults_func_t config_defaults = config_defaults_zerogw;
    read_options_func_t read_options = read_options_zerogw;
    char *config_name = "Zerogw";
    state_info_t *config_states = states_zerogw;
    config_meta_t config_meta = {
        config_defaults: config_defaults_zerogw,
        read_options: read_options_zerogw,
        service_name: "Zerogw",
        config_states: states_zerogw,
        };
#endif