
/* WARNING: THIS IS AUTOMATICALLY GENERATED FILE, DO NOT EDIT! */
#ifndef _H_CFG_ZEROGW
#define _H_CFG_ZEROGW

#include <stddef.h>
#include "configbase.h"

typedef struct config_zerogw_s {
    config_head_t head;
    struct {
        
        struct {
            char * access; size_t access_len;
            int level;
            int warning_timeout;
            char * error; size_t error_len;
        } logging;
        
        struct {
            int max_body_size;
            
            struct config_zerogw_method_s *method;
        } limits;
        
        struct {
            ;
            char * element; size_t element_len;
        } contents;
        
        struct {
            
            struct {
                char * status; size_t status_len;
                char * body; size_t body_len;
                int code;
            } uri_not_found;
            
            struct {
                char * status; size_t status_len;
                char * body; size_t body_len;
                int code;
            } internal_error;
            
            struct {
                char * status; size_t status_len;
                char * body; size_t body_len;
                int code;
            } domain_not_found;
        } responses;
    } Globals;
    struct config_zerogw_Pages_s *Pages;
    struct {
        
        struct config_zerogw_listen_s *listen;
    } Server;
} config_zerogw_t;


typedef struct config_zerogw_method_s {
    array_element_t head;
    char * value; size_t value_len;
} config_zerogw_method_t;

typedef struct config_zerogw_Pages_s {
    mapping_element_t head;
    
    struct {
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } uri_not_found;
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } internal_error;
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } domain_not_found;
    } responses;
    
    struct {
        char * access; size_t access_len;
        int level;
        int warning_timeout;
        char * error; size_t error_len;
    } logging;
    
    struct config_zerogw_pages_s *pages;
    
    struct {
        int max_body_size;
        
        struct config_zerogw_method_s *method;
    } limits;
    
    struct {
        ;
        char * element; size_t element_len;
    } contents;
    
} config_zerogw_Pages_t;

typedef struct config_zerogw_listen_s {
    array_element_t head;
    char * host; size_t host_len;
    int port;
    
} config_zerogw_listen_t;

typedef struct config_zerogw_pages_s {
    array_element_t head;
    
    struct {
        char * access; size_t access_len;
        int level;
        int warning_timeout;
        char * error; size_t error_len;
    } logging;
    
    struct {
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } uri_not_found;
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } internal_error;
        
        struct {
            char * status; size_t status_len;
            char * body; size_t body_len;
            int code;
        } domain_not_found;
    } responses;
    
    struct {
        int max_body_size;
        
        struct config_zerogw_method_s *method;
    } limits;
    
    struct config_zerogw_forward_s *forward;
    char * path; size_t path_len;
    
    struct {
        ;
        char * element; size_t element_len;
    } contents;
    
} config_zerogw_pages_t;

typedef struct config_zerogw_forward_s {
    array_element_t head;
    char * value; size_t value_len;
} config_zerogw_forward_t;


#if defined(CONFIG_ZEROGW)
    typedef config_zerogw_t config_t;
    extern config_t config;
#endif
#endif