#include "websocket.h"

typedef struct subscriber_s {
    struct subscriber_s *topic_next;
    struct subscriber_s *topic_prev;
    struct subscriber_s *client_next;
    struct subscriber_s *client_prev;
    connection_t *conn;
} subscriber_t;

typedef struct topic_s {
    struct topic_s *next;
    struct topic_s *prev;
    subscriber_t *first_sub;
    subscriber_t *last_sub;
    char topic[];
} topic_t;

typedef struct topic_hash_s {
    int hash_size;
    int ntopics;
    int nsubscriptions;
    topic_t *table[];
} topic_hash_t;

void websock_start(connection_t *conn, config_Route_t *route) {
    conn->route = route;
}
int websock_message(connection_t *conn, message_t *msg) {
    printf("MESSAGE\n");
}
