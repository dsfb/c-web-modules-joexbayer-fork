#ifndef CWEB_H
#define CWEB_H

#include <http.h>
#include <map.h>
#include <container.h>
#include <scheduler.h>
#include <db.h>

/* Macro to export a module_t configuration */
#define export __attribute__((visibility("default"))) const 

typedef int (*entry_t)(struct http_request *, struct http_response *);
typedef enum {
    FEATURE_FLAG_NONE = 0,
} cweb_feature_flag_t;

/* Websocket information */
typedef struct {
    const char *path;
    void (*on_open)(struct websocket *);  
    void (*on_message)(struct websocket *, const char *message, size_t length);
    void (*on_close)(struct websocket *); 
} websocket_info_t;

/* Route information */
typedef struct route_info {
    const char *path;
    const char *method;
    entry_t handler;
    int flags;
} route_info_t;

/* Module information */
typedef struct module {
    char name[128];
    char author[128];
    route_info_t routes[10];
    int size;
    websocket_info_t websockets[10];
    int ws_size
} module_t;

/* Exposed primitives */
extern struct container* container;
extern struct scheduler* scheduler;
extern struct sqldb* database;
// KeyValue store
// Queues
// Authentication/Sessions.
// Config
// Logging

#endif // CWEB_H