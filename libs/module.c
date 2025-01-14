#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <container.h>
#include <scheduler.h>
#include <db.h>

/* Macro loads a symbol from the "parent" and exposes it as given variable. */
#define LOAD_SYMBOL(handle, symbol, type, var) \
    do { \
        type** var##_ptr = (type**)dlsym(handle, symbol); \
        if (!var##_ptr) { \
            fprintf(stderr, "Error accessing " #var "_ptr: %s\n", dlerror()); \
            return; \
        } \
        var = *var##_ptr; \
        if (!var) { \
            fprintf(stderr, "Error accessing " #var ": %s\n", dlerror()); \
            return; \
        } \
    } while (0)

struct scheduler* scheduler = NULL;
struct sqldb *database = NULL;
struct container* cache = NULL;
/* Global handle to access server symbols */
static void *dlhandle = NULL;

__attribute__((constructor)) void module_constructor() {
    dlhandle = dlopen(NULL, RTLD_GLOBAL | RTLD_LAZY);
    if (!dlhandle) {
        fprintf(stderr, "Error accessing server symbols: %s\n", dlerror());
        return;
    }

    LOAD_SYMBOL(dlhandle, "exposed_container", struct container, cache);
    LOAD_SYMBOL(dlhandle, "exposed_scheduler", struct scheduler, scheduler);
    LOAD_SYMBOL(dlhandle, "exposed_sqldb", struct sqldb, database);
}