
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>
#include <openssl/crypto.h>

#include "http.h"
#include "router.h"
#include "cweb.h"
#include "map.h"
#include "db.h"
#include "scheduler.h"

#define MODULE_URL "/mgnt"

/* Feature for later... */
static const char* allowed_management_commands[] = {
    "reload",
    "shutdown",
    "status",
    "routes",
    "modules",
    "help"
};

static const char* allowed_ip_prefixes[] = {
    "192.168.",
    "10.0.",
    "172.16."
};

struct cidr_prefix {
    uint32_t prefix;    // Integer representation of the prefix
    uint8_t prefix_len; // Length of the prefix in bits
};

struct connection {
    int sockfd;
    struct sockaddr_in address;
};

// static int parse_cidr(const char *cidr_str, struct cidr_prefix *result) {
//     char ip[INET_ADDRSTRLEN];
//     int prefix_len;

//     if (sscanf(cidr_str, "%15[^/]/%d", ip, &prefix_len) != 2) {
//         fprintf(stderr, "Invalid CIDR format: %s\n", cidr_str);
//         return -1;
//     }

//     if (prefix_len < 0 || prefix_len > 32) {
//         fprintf(stderr, "Invalid prefix length: %d\n", prefix_len);
//         return -1;
//     }

//     struct in_addr addr;
//     if (inet_pton(AF_INET, ip, &addr) != 1) {
//         fprintf(stderr, "Invalid IP address: %s\n", ip);
//         return -1;
//     }

//     result->prefix = ntohl(addr.s_addr); // Convert to host byte order
//     result->prefix_len = (uint8_t)prefix_len;

//     return 0;
// }

/* TODO: Ugly fix to allow server access to these.. */
void ws_handle_client(int sd, struct http_request *req, struct http_response *res, struct ws_info *ws_module_info);
int ws_confirm_open(int sd);

static struct connection server_init(uint16_t port) {
    struct connection s;
    s.sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (s.sockfd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(s.sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(s.sockfd);
        exit(EXIT_FAILURE);
    }

    s.address.sin_family = AF_INET;
    s.address.sin_addr.s_addr = INADDR_ANY;
    s.address.sin_port = htons(port);

    if (bind(s.sockfd, (struct sockaddr *)&s.address, sizeof(s.address)) < 0) {
        perror("Bind failed");
        close(s.sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(s.sockfd, 3) < 0) {
        perror("Listen failed");
        close(s.sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", port);
    return s;
}

static struct connection* server_accept(struct connection s) {
    struct connection *c = (struct connection *)malloc(sizeof(struct connection));
    if (c == NULL) {
        perror("Error allocating memory for client");
        close(s.sockfd);
        exit(EXIT_FAILURE);
    }

    int addrlen = sizeof(s.address);
    c->sockfd = accept(s.sockfd, (struct sockaddr *)&s.address, (socklen_t *)&addrlen);
    if (c->sockfd < 0) {
        perror("Accept failed");
        free(c);
        return NULL;
    }

    return c;
}

/* TODO: Run in seprate isolated process. */
static int gateway(int fd, struct http_request *req, struct http_response *res) {
    if (strncmp(req->path, "/favicon.ico", 12) == 0) {
        res->status = HTTP_404_NOT_FOUND;
        snprintf(res->body, HTTP_RESPONSE_SIZE, "404 Not Found\n");
        return 0;
    }

    if(strncmp(req->path, MODULE_URL, 6) == 0) {
        if(mgnt_parse_request(req, res) >= 0) {
            res->status = HTTP_200_OK; 
        } else {
            res->status = HTTP_500_INTERNAL_SERVER_ERROR;
        }
        return 0;
    }

    if(http_is_websocket_upgrade(req)) {
        struct ws_route ws = ws_route_find(req->path);
        if (ws.info == NULL) {
            res->status = HTTP_404_NOT_FOUND;
            snprintf(res->body, HTTP_RESPONSE_SIZE, "404 Not Found\n");
            return 0;
        }

        /* Upgrade to websocket */
        ws_handle_client(fd, req, res, ws.info);

        pthread_rwlock_unlock(ws.rwlock);

        return 0;
    }

    struct route r = route_find(req->path, (char*)http_methods[req->method]);
    if (r.route == NULL) {
        res->status = HTTP_404_NOT_FOUND;
        snprintf(res->body, HTTP_RESPONSE_SIZE, "404 Not Found\n"); 
        return 0;
    }

    safe_execute_handler(r.route->handler, req, res);

    /* Release the read lock after handler execution */
    pthread_rwlock_unlock(r.rwlock);
    return 0;
}

/* Build headers for response */
static void build_headers(struct http_response *res, char *headers, int headers_size) {
    struct map *headers_map = res->headers;
    int headers_len = 0;
    for (size_t i = 0; i < map_size(headers_map); i++) {
        int written = snprintf(headers + headers_len, headers_size - headers_len, "%s: %s\r\n", headers_map->entries[i].key, (char*)headers_map->entries[i].value);
        if (written < 0 || written >= headers_size - headers_len) {
            fprintf(stderr, "Header buffer overflow\n");
            break;
        }
        headers_len += written;
    }
}

static void measure_time(struct timespec *start, struct timespec *end, double *time_taken) {
    clock_gettime(CLOCK_MONOTONIC, end);
    *time_taken = (end->tv_sec - start->tv_sec) * 1e9;
    *time_taken = (*time_taken + (end->tv_nsec - start->tv_nsec)) * 1e-9;
}

static void clean_up(struct http_request *req, struct http_response *res) {
    if (req->body) free(req->body);

    for (size_t i = 0; i < map_size(req->params); i++) {
        free(req->params->entries[i].value);
    }
    map_destroy(req->params);

    for (size_t i = 0; i < map_size(req->headers); i++) {
        free(req->headers->entries[i].value);
    }
    map_destroy(req->headers);
    map_destroy(req->data);
    map_destroy(res->headers);
}

static void *thread_handle_client(void *arg) {
    struct connection *c = (struct connection *)arg;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("[%ld] Handling client %d\n", (long)pthread_self(), c->sockfd);

    char buffer[8*1024] = {0};
    int read_size = read(c->sockfd, buffer, sizeof(buffer) - 1);
    if (read_size <= 0) {
        close(c->sockfd);
        free(c);
        return NULL;
    }
    buffer[read_size] = '\0';

    struct http_request req;
    req.tid = pthread_self();

    http_parse(buffer, &req);

    while (req.content_length > read_size) {
        read_size += read(c->sockfd, buffer + read_size, sizeof(buffer) - read_size - 1);
        buffer[read_size] = '\0';
    }
    req.body = strdup(strstr(buffer, "\r\n\r\n") + 4);

    http_parse_data(&req);

    printf("[%ld] %d Request:\n", (long)req.tid, req.content_length);

    struct http_response res;
    res.headers = map_create(32);
    if (res.headers == NULL) {
        perror("Error creating map");
        close(c->sockfd);
        free(c);
        return NULL;
    }

    res.body = (char *)malloc(HTTP_RESPONSE_SIZE);
    if (res.body == NULL) {
        perror("Error allocating memory for response body");
        close(c->sockfd);
        free(c);
        return NULL;
    }

    gateway(c->sockfd, &req, &res);

    char headers[4*1024] = {0};
    build_headers(&res, headers, sizeof(headers));
    if (!req.websocket) {
        map_insert(res.headers, "Connection", "close");
    }

    char response[8*1024] = {0};
    snprintf(response, sizeof(response), HTTP_VERSION" %s\r\n%sContent-Length: %lu\r\n\r\n%s", http_errors[res.status], headers, strlen(res.body), res.body);
    write(c->sockfd, response, strlen(response));

    double time_taken;
    measure_time(&start, &end, &time_taken);

    printf("[%ld] Request %s %s took %f seconds\n", (long)req.tid, http_methods[req.method], req.path, time_taken);

    char* ac = map_get(res.headers, "Sec-WebSocket-Accept");
    if (ac) free(ac);

    clean_up(&req, &res);

    if (!req.websocket) {
        printf("[%ld] Closing connection (not a websocket)\n", (long)req.tid);
        close(c->sockfd);
    } else {
        ws_confirm_open(c->sockfd);
    }

    free(c);
    free(res.body);
    return NULL;
}

#define INIT_OPTIONS (OPENSSL_INIT_NO_ATEXIT)
static void openssl_init_wrapper(void) {
    if (OPENSSL_init_crypto(INIT_OPTIONS, NULL) == 0) {
        fprintf(stderr, "Failed to initialize OpenSSL\n");
        exit(EXIT_FAILURE);
    }
}

/* Signal handler */
static volatile sig_atomic_t stop = 0;
static void server_signal_handler(int sig) {
(void)sig;
    stop = 1;
}

int main() {
    (void)allowed_management_commands;
    (void)allowed_ip_prefixes;

    CRYPTO_ONCE openssl_once = CRYPTO_ONCE_STATIC_INIT;
    if (!CRYPTO_THREAD_run_once(&openssl_once, openssl_init_wrapper)) {
        fprintf(stderr, "Failed to run OpenSSL initialization\n");
        exit(EXIT_FAILURE);
    }

    printf("Starting server...\n");
    
    struct sigaction sa;
    sa.sa_handler = server_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct connection s = server_init(8080);

    while (!stop) {
        struct connection *client = server_accept(s);
        if (client == NULL) {
            if (errno == EINTR && stop) {
                break;
            }
            perror("Error accepting client");
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, thread_handle_client, client) != 0) {
            perror("Error creating thread");
            close(client->sockfd);
            free(client);
            continue;
        }
        pthread_detach(thread);
    }


    close(s.sockfd);
    printf("Server shutting down gracefully.\n");

    return 0;
}