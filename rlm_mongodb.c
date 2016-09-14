#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <mongoc.h>

enum {
  RLM_MONGODB_GET,
  RLM_MONGODB_SET
};

typedef struct rlm_mongodb_t {
  struct {
    const char *action;
    const char *server;
    vp_tmpl_t *db;
    vp_tmpl_t *collection;
    vp_tmpl_t *search_query;
    vp_tmpl_t *sort_query;
    vp_tmpl_t *update_query;
    bool remove;
    bool upsert;
  } cfg;

  int action;
  const char *name;
  fr_connection_pool_t *pool;
} rlm_mongodb_t;

typedef struct rlm_mongodb_conn {
  mongoc_client_t *client;
} rlm_mongodb_conn_t;

static const CONF_PARSER module_config[] = {
    {"action", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_NOT_EMPTY, rlm_mongodb_t, cfg.action), NULL},
    {"server", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_SECRET | PW_TYPE_NOT_EMPTY, rlm_mongodb_t, cfg.server), NULL},
    {"db", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_TMPL | PW_TYPE_NOT_EMPTY, rlm_mongodb_t, cfg.db), NULL},
    {"collection", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_TMPL | PW_TYPE_NOT_EMPTY, rlm_mongodb_t, cfg.collection),
     NULL},
    {"search_query", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_TMPL, rlm_mongodb_t, cfg.search_query), ""},
    {"sort_query", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_TMPL, rlm_mongodb_t, cfg.sort_query), ""},
    {"update_query", FR_CONF_OFFSET(PW_TYPE_STRING | PW_TYPE_TMPL, rlm_mongodb_t, cfg.update_query), ""},
    {"remove", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_mongodb_t, cfg.remove), "no"},
    {"upsert", FR_CONF_OFFSET(PW_TYPE_BOOLEAN, rlm_mongodb_t, cfg.upsert), "no"},
    CONF_PARSER_TERMINATOR
};

/**
 * MongoDB log handler.
 * @param[in] log_level Log level.
 * @param[in] log_domain Log domain.
 * @param[in] message Message string.
 * @param[in] user_data Module instance.
 */
static void mongoc_log_handler(mongoc_log_level_t log_level,
                               const char *log_domain,
                               const char *message,
                               void *user_data) {
  rlm_mongodb_t *inst = user_data;
  (void) log_level;

  INFO("rlm_mongodb (%s): %s: %s", inst->name, log_domain, message);
}

/**
 * Detach module.
 * @param[in] instance Module instance.
 * @return
 */
static int mod_detach(void *instance) {
  rlm_mongodb_t *inst = instance;
  fr_connection_pool_free(inst->pool);
  // do not call mongoc_cleanup as other instances can be still working.
  return 0;
}

/**
 * Module connection destructor.
 * @param[in] conn Connection handle.
 * @return Zero on success.
 */
static int mod_conn_free(rlm_mongodb_conn_t *conn) {
  if (conn->client) mongoc_client_destroy(conn->client);
  DEBUG("rlm_mongodb: closed connection");
  return 0;
}

/**
 * Module connection constructor.
 * @param[in] ctx Talloc context.
 * @param[in] instance Module instance.
 * @return NULL on error, else a connection handle.
 */
static void *mod_conn_create(TALLOC_CTX *ctx, void *instance) {
  rlm_mongodb_t *inst = instance;

  mongoc_client_t *client = mongoc_client_new(inst->cfg.server);
  if (!client) {
    ERROR("rlm_mongodb (%s): failed to connect to server", inst->name);
    return NULL;
  }

  rlm_mongodb_conn_t *conn = talloc_zero(ctx, rlm_mongodb_conn_t);
  conn->client = client;
  talloc_set_destructor(conn, mod_conn_free);

  return conn;
}

/**
 * Instantiate module.
 * @param[in] conf Module config.
 * @param[in] instance Module instance.
 * @return Zero on success.
 */
static int mod_instantiate(CONF_SECTION *conf, void *instance) {
  rlm_mongodb_t *inst = instance;
  bool ok = true;

  inst->name = cf_section_name2(conf);
  if (!inst->name) {
    inst->name = cf_section_name1(conf);
  }

  if (!strcasecmp(inst->cfg.action, "get")) {
    inst->action = RLM_MONGODB_GET;
    cf_log_err_cs(conf, "action 'get' is not implemented");
    ok = false;
  } else if (!strcasecmp(inst->cfg.action, "set")) {
    inst->action = RLM_MONGODB_SET;
  } else {
    cf_log_err_cs(conf, "invalid 'action', use'get' or 'set'");
    ok = false;
  }

  if (inst->cfg.remove && inst->cfg.update_query) {
    cf_log_err_cs(conf, "'update_query' and 'remove' can't be used at the same time");
    ok = false;
  } else if (!inst->cfg.remove && !inst->cfg.update_query) {
    cf_log_err_cs(conf, "'update_query' or 'remove' must be set for 'set' action");
    ok = false;
  }

  mongoc_init();
  mongoc_log_set_handler(mongoc_log_handler, inst);

  inst->pool = fr_connection_pool_module_init(conf, inst, mod_conn_create, NULL, inst->name);
  if (!inst->pool) {
    ok = false;
  }

  return ok ? 0 : -1;
}

/**
 * Main module procedure.
 * @param[in] instance Module instance.
 * @param[in] request Radius request.
 * @return
 */
static rlm_rcode_t mod_proc(void *instance, REQUEST *request) {
  rlm_mongodb_t *inst = instance;
  rlm_mongodb_conn_t *conn = NULL;
  rlm_rcode_t code = RLM_MODULE_FAIL;
  bson_error_t error;

  conn = fr_connection_get(inst->pool);
  if (!conn) {
    goto end;
  }

  if (inst->action == RLM_MONGODB_GET) {
    // TODO: implement me!
    code = RLM_MODULE_NOOP;
  } else {
    mongoc_collection_t *mongo_collection = NULL;
    char *db = NULL, *collection = NULL, *query = NULL, *sort = NULL, *update = NULL;
    bson_t *bson_query = NULL, *bson_sort = NULL, *bson_update = NULL;

    if (tmpl_aexpand(request, &db, request, inst->cfg.db, NULL, NULL) < 0) {
      ERROR("failed to substitute attributes for db '%s'", inst->cfg.db->name);
      goto end_set;
    }

    if (tmpl_aexpand(request, &collection, request, inst->cfg.collection, NULL, NULL) < 0) {
      ERROR("failed to substitute attributes for collection '%s'", inst->cfg.collection->name);
      goto end_set;
    }

    ssize_t query_len = tmpl_aexpand(request, &query, request, inst->cfg.search_query, NULL, NULL);
    if (query_len < 0) {
      ERROR("failed to substitute attributes for search query '%s'", inst->cfg.search_query->name);
      goto end_set;
    }
    bson_query = bson_new_from_json((uint8_t *) query, query_len, &error);
    if (!bson_query) {
      RERROR("JSON->BSON conversion failed for search query '%s': %d.%d %s",
             query, error.domain, error.code, error.message);
      goto end_set;
    }

    ssize_t sort_len = tmpl_aexpand(request, &sort, request, inst->cfg.sort_query, NULL, NULL);
    if (query_len < 0) {
      ERROR("failed to substitute attributes for sort query '%s'", inst->cfg.sort_query->name);
      goto end_set;
    }
    if (sort_len) {
      bson_sort = bson_new_from_json((uint8_t *) sort, sort_len, &error);
      if (!bson_sort) {
        RERROR("JSON->BSON conversion failed for sort query '%s': %d.%d %s",
               sort, error.domain, error.code, error.message);
        goto end_set;
      }
    }

    ssize_t update_len = tmpl_aexpand(request, &update, request, inst->cfg.update_query, NULL, NULL);
    if (query_len < 0) {
      ERROR("failed to substitute attributes for update query '%s'", inst->cfg.update_query->name);
      goto end_set;
    }
    if (update_len) {
      bson_update = bson_new_from_json((uint8_t *) update, update_len, &error);
      if (!bson_update) {
        RERROR("JSON->BSON conversion failed for update query '%s': %d.%d %s",
               update, error.domain, error.code, error.message);
        goto end_set;
      }
    }

    mongo_collection = mongoc_client_get_collection(conn->client, db, collection);
    if (!mongo_collection) {
      RERROR("failed to get collection %s/%s", db, collection);
      goto end_set;
    }

    bool ok = mongoc_collection_find_and_modify(mongo_collection, bson_query, bson_sort,
                                                bson_update, NULL, inst->cfg.remove,
                                                inst->cfg.upsert, false, NULL, &error);

    code = ok ? RLM_MODULE_OK : RLM_MODULE_FAIL;

    end_set:
    if (mongo_collection) mongoc_collection_destroy(mongo_collection);
    if (bson_query) bson_destroy(bson_query);
    if (bson_sort) bson_destroy(bson_sort);
    if (bson_update) bson_destroy(bson_update);
  }

  end:
  if (conn) fr_connection_release(inst->pool, conn);
  return code;
}

/* globally exported name */
extern module_t rlm_mongodb;
module_t rlm_mongodb = {
    .magic = RLM_MODULE_INIT,
    .name = "mongodb",
    .type = RLM_TYPE_THREAD_SAFE | RLM_TYPE_HUP_SAFE,
    .inst_size = sizeof(rlm_mongodb_t),
    .config = module_config,
    .instantiate = mod_instantiate,
    .detach = mod_detach,
    .methods = {
        [MOD_AUTHENTICATE] = mod_proc,
        [MOD_AUTHORIZE] = mod_proc,
        [MOD_PREACCT] = mod_proc,
        [MOD_ACCOUNTING] = mod_proc,
        [MOD_SESSION] = NULL,
        [MOD_PRE_PROXY] = mod_proc,
        [MOD_POST_PROXY] = mod_proc,
        [MOD_POST_AUTH] = mod_proc,
#ifdef WITH_COA
        [MOD_RECV_COA] = mod_proc,
        [MOD_SEND_COA] = mod_proc,
#endif
    },
};
