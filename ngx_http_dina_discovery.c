#include "ngx_http_dina_discovery.h"
#include <zookeeper.h>
#include <semaphore.h>
#include <pthread.h>

static void zookeeper_watcher(zhandle_t *, int, int, const char *, void *);
static void zookeeper_strings_completion(int, const struct String_vector *, const void *);

struct strings_completion_result {
    sem_t *sem;
    ngx_str_t *result;
};

int ngx_http_dina_discovery(ngx_str_t *const result, const ngx_str_t *const zoo_addr, const ngx_str_t *const service_name) {
    zhandle_t *zh;
    sem_t sem;
    struct strings_completion_result sc = { &sem, result };
    sem_init(&sem, 0, 0);

    zoo_deterministic_conn_order(1);
    zh = zookeeper_init((const char *) zoo_addr->data, zookeeper_watcher, 30000, 0, NULL, 0);
    if (!zh) {
        return -1;
    }

    zoo_aget_children(zh, (const char *) service_name->data, 1, zookeeper_strings_completion, &sc);
    sem_wait(&sem);

    zookeeper_close(zh);
    return 0;
}

static void zookeeper_watcher(zhandle_t *zk, int type, int state, const char *path, void *ctx) {
    (void) zk;
    (void) type;
    (void) state;
    (void) path;
    (void) ctx;
}

static void zookeeper_strings_completion(int rc, const struct String_vector *strings, const void *data) {
    struct strings_completion_result *sc = (struct strings_completion_result *) data;
    if (rc != 0) {
        goto finish;
    }
    if (strings) {
        if (strings->count == 0) {
            goto finish;
        }
        const char *choosed = strings->data[rand() % strings->count];
        sc->result->len = strlen(choosed);
        memcpy(sc->result->data, choosed, sc->result->len);
    }
finish:
    sem_post(sc->sem);
}

