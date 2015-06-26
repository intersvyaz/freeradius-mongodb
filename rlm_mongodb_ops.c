#include <stdbool.h>
#include <semaphore.h>

#ifdef STANDALONE_BUILD

#include <freeradius/radiusd.h>
#include <freeradius/modules.h>

#else
#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#endif

#include <mongoc.h>
#include "utlist.h"

#define MAX_QUERY_LEN 2048

enum
{
    RLM_MONGODB_OPS_GET,
    RLM_MONGODB_OPS_SET
};

struct rlm_mongodb_ops_t;

typedef struct mongodb_ops_req_t
{
    unsigned int id;
    bson_t *bson_query;
    bson_t *bson_sort;
    bson_t *bson_update;
    struct mongodb_ops_req_t *next;
    struct mongodb_ops_req_t *prev;
} mongodb_ops_req_t;

typedef struct mongodb_ops_thread_t
{
    pthread_t handle;
    struct rlm_mongodb_ops_t *instance;
    mongoc_client_t *client;
    mongoc_collection_t *collection;
} mongodb_ops_thread_t;

typedef struct rlm_mongodb_ops_t
{
    struct
    {
        /* server address, like "mongodb://127.0.0.1:27017/" */
        char *server;
        /* thread pool size */
        int pool_size;
        /* get or set */
        char *action;
        /* database name */
        char *db;
        /* collection name */
        char *collection;
        /* search query */
        char *search_query;
        /* sort query */
        char *sort_query;
        /* update query */
        char *update_query;
        /* preform remove */
        bool remove;
        /* preform upsert */
        bool upsert;
    } cfg;

    int action;
    volatile bool stop;
    mongodb_ops_thread_t *pool;
    mongodb_ops_req_t *queue;
    sem_t queue_sem;
    pthread_mutex_t queue_lock;
} rlm_mongodb_ops_t;

static CONF_PARSER module_config[] = {
        {"server",       PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.server),       NULL, ""},
        {"pool_size",    PW_TYPE_INTEGER,    offsetof(rlm_mongodb_ops_t, cfg.pool_size),    NULL, "1"},
        {"action",       PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.action),       NULL, ""},
        {"db",           PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.db),           NULL, ""},
        {"collection",   PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.collection),   NULL, ""},
        {"search_query", PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.search_query), NULL, ""},
        {"sort_query",   PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.sort_query),   NULL, ""},
        {"update_query", PW_TYPE_STRING_PTR, offsetof(rlm_mongodb_ops_t, cfg.update_query), NULL, ""},
        {"remove",       PW_TYPE_BOOLEAN,    offsetof(rlm_mongodb_ops_t, cfg.remove),       NULL, "no"},
        {"upsert",       PW_TYPE_BOOLEAN,    offsetof(rlm_mongodb_ops_t, cfg.upsert),       NULL, "yes"},
        {NULL, -1, 0,                                                                       NULL, NULL}
};

static int mongodb_ops_detach(void *instance);

static void *mongodb_ops_thread_proc(void *arg);

static int mongodb_ops_instantiate(CONF_SECTION *conf, void **instance)
{
    rlm_mongodb_ops_t *inst;

    inst = rad_malloc(sizeof(rlm_mongodb_ops_t));
    if (!inst) {
        return -1;
    }
    memset(inst, 0, sizeof(*inst));

    if (cf_section_parse(conf, inst, module_config) < 0) {
        goto err;
    }

    if (inst->cfg.pool_size <= 0) {
        radlog(L_ERR, "rlm_mongodb_ops: Invalid thread pool size '%d'", inst->cfg.pool_size);
        goto err;
    }

    if (strcasecmp(inst->cfg.action, "get") == 0) {
        inst->action = RLM_MONGODB_OPS_GET;
        radlog(L_ERR, "rlm_mongodb_ops: Action get is not implemented, sorry");
        goto err;
    } else if (strcasecmp(inst->cfg.action, "set") == 0) {
        inst->action = RLM_MONGODB_OPS_SET;
    } else {
        radlog(L_ERR, "rlm_mongodb_ops: Invalid action '%s', only 'get' or 'set' is acceptable", inst->cfg.action);
        goto err;
    }

    if (inst->cfg.remove && ('\0' != inst->cfg.update_query)) {
        radlog(L_ERR, "rlm_mongodb_ops: Update query and remove flag can't be used at the same time");
        goto err;
    }

    mongoc_init();
    pthread_mutex_init(&inst->queue_lock, NULL);
    sem_init(&inst->queue_sem, 0, 0);

    inst->pool = rad_malloc(sizeof(*inst->pool) * inst->cfg.pool_size);
    memset(inst->pool, 0, sizeof(*inst->pool) * inst->cfg.pool_size);
    int i;
    for (i = 0; i < inst->cfg.pool_size; i++) {
        mongodb_ops_thread_t *thread = &inst->pool[i];
        thread->client = mongoc_client_new(inst->cfg.server);
        thread->instance = inst;
        if (!thread->client) {
            radlog(L_ERR, "rlm_mongodb_ops: Failed to connect to %s", inst->cfg.server);
            goto err;
        }
        thread->collection = mongoc_client_get_collection(thread->client, inst->cfg.db, inst->cfg.collection);
        if (!thread->collection) {
            radlog(L_ERR, "rlm_mongodb_ops: Failed to connect to %s", inst->cfg.server);
            goto err;
        }
        if (0 != pthread_create(&thread->handle, NULL, mongodb_ops_thread_proc, thread)) {
            radlog(L_ERR, "rlm_mongodb_ops: Failed to spawn thread");
            goto err;
        }
    }

    *instance = inst;
    return 0;

    err:
    mongodb_ops_detach(inst);
    return -1;
}

static int mongodb_ops_detach(void *instance)
{
    rlm_mongodb_ops_t *inst = instance;
    int i;

    inst->stop = true;
    sem_destroy(&inst->queue_sem);
    pthread_mutex_destroy(&inst->queue_lock);

    for (i = 0; i < inst->cfg.pool_size; i++) {
        pthread_join(inst->pool[i].handle, NULL);
        if (inst->pool[i].collection) mongoc_collection_destroy(inst->pool[i].collection);
        if (inst->pool[i].client) mongoc_client_destroy(inst->pool[i].client);
    }
    free(inst->pool);

    // clean queue

    mongoc_cleanup();
    free(instance);

    return 0;
}

static int mongodb_ops_proc(void *instance, REQUEST *request)
{
    rlm_mongodb_ops_t *inst = instance;

    if (RLM_MONGODB_OPS_SET == inst->action) {
        bson_error_t error;
        char query[MAX_QUERY_LEN], sort[MAX_QUERY_LEN], update[MAX_QUERY_LEN];
        bson_t *bson_query = NULL;
        bson_t *bson_sort = NULL;
        bson_t *bson_update = NULL;
        int query_len = 0, sort_len = 0, update_len = 0;

        query[0] = sort[0] = update[0] = '\0';

        query_len = radius_xlat(query, sizeof(query), inst->cfg.search_query, request, NULL);
        bson_query = bson_new_from_json((uint8_t *) query, query_len, &error);
        if (!bson_query) {
            radlog(L_ERR, "rlm_mongodb_ops: JSON->BSON conversion failed for search query '%s': %d.%d %s",
                   query, error.domain, error.code, error.message);
            goto end_set;
        }

        if ('\0' != inst->cfg.sort_query[0]) {
            sort_len = radius_xlat(sort, sizeof(sort), inst->cfg.sort_query, request, NULL);
            bson_sort = bson_new_from_json((uint8_t *) sort, sort_len, &error);
            if (!bson_sort) {
                radlog(L_ERR, "rlm_mongodb_ops: JSON->BSON conversion failed for sort query '%s': %d.%d %s",
                       sort, error.domain, error.code, error.message);
                goto end_set;
            }
        }

        if ('\0' != inst->cfg.update_query[0]) {
            update_len = radius_xlat(update, sizeof(update), inst->cfg.update_query, request, NULL);
            bson_update = bson_new_from_json((uint8_t *) update, update_len, &error);
            if (!bson_update) {
                radlog(L_ERR, "rlm_mongodb_ops: JSON->BSON conversion failed for update query '%s': %d.%d %s",
                       update, error.domain, error.code, error.message);
                goto end_set;
            }
        }

        mongodb_ops_req_t *mreq = rad_malloc(sizeof(*mreq));
        mreq->bson_query = bson_query;
        mreq->bson_sort = bson_sort;
        mreq->bson_update = bson_update;
        mreq->id = request->number;
        pthread_mutex_lock(&inst->queue_lock);
        DL_APPEND(inst->queue, mreq);
        pthread_mutex_unlock(&inst->queue_lock);
        sem_post(&inst->queue_sem);
        DEBUG2("rlm_mongodb_ops: Queued request %u query='%s, sort='%s', update='%s'", mreq->id, query, sort, update);

        return RLM_MODULE_NOOP;

        end_set:
        if (bson_query) bson_destroy(bson_query);
        if (bson_sort) bson_destroy(bson_sort);
        if (bson_update) bson_destroy(bson_update);
    } else {
        return RLM_MODULE_NOOP;
    }

    return RLM_MODULE_NOOP;
}

static void *mongodb_ops_thread_proc(void *arg)
{
    bson_error_t error;
    mongodb_ops_thread_t *self = (mongodb_ops_thread_t *) arg;

    while (!self->instance->stop) {
        if (0 != sem_wait(&self->instance->queue_sem)) {
            break;
        }
        if (0 != pthread_mutex_lock(&self->instance->queue_lock)) {
            break;
        }

        mongodb_ops_req_t *mreq = self->instance->queue;
        DL_DELETE(self->instance->queue, mreq);

        pthread_mutex_unlock(&self->instance->queue_lock);

        bool success = mongoc_collection_find_and_modify(self->collection, mreq->bson_query, mreq->bson_sort,
                                                         mreq->bson_update, NULL, self->instance->cfg.remove,
                                                         self->instance->cfg.upsert, false, NULL, &error);
        if (!success) {
            radlog(L_ERR, "rlm_mongodb_ops: set operation failed for request %u: %d.%d %s", mreq->id,
                   error.domain, error.code, error.message);
        } else {
            DEBUG2("rlm_mongodb_ops: set operation success for request %u", mreq->id);
        }

        if (mreq->bson_query) bson_destroy(mreq->bson_query);
        if (mreq->bson_sort) bson_destroy(mreq->bson_sort);
        if (mreq->bson_update) bson_destroy(mreq->bson_update);
        free(mreq);
    }

    return NULL;
}

/* globally exported name */
module_t rlm_mongodb_ops = {
        RLM_MODULE_INIT,
        "mongodb_ops",
        RLM_TYPE_THREAD_SAFE,
        mongodb_ops_instantiate,   /* instantiation */
        mongodb_ops_detach,        /* detach */
        {
                mongodb_ops_proc,  /* authentication */
                mongodb_ops_proc,  /* authorization */
                mongodb_ops_proc,  /* preaccounting */
                mongodb_ops_proc,  /* accounting */
                NULL,              /* checksimul */
                mongodb_ops_proc,  /* pre-proxy */
                mongodb_ops_proc,  /* post-proxy */
                mongodb_ops_proc   /* post-auth */
        },
};
