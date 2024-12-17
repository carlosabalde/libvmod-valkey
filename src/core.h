#ifndef CORE_H_INCLUDED
#define CORE_H_INCLUDED

#include <syslog.h>
#include <pthread.h>
#include <valkey/valkey.h>
#ifdef TLS_ENABLED
#include <valkey/tls.h>
#endif
#include <netinet/in.h>
#include <inttypes.h>

#include "vqueue.h"

#define NVALKEY_SERVER_ROLES 3
#define NVALKEY_SERVER_WEIGHTS 4
#define NVALKEY_CLUSTER_SLOTS 16384

enum VALKEY_PROTOCOL {
    VALKEY_PROTOCOL_DEFAULT = 0,
    VALKEY_PROTOCOL_RESP2 = 2,
    VALKEY_PROTOCOL_RESP3 = 3
};

// Required lock ordering to avoid deadlocks:
//   1. vcl_state->mutex.
//   2. vmod_valkey_db->mutex.

// WARNING: ordering of roles in this enumeration is relevant when populating
// an execution plan.
enum VALKEY_SERVER_ROLE {
    VALKEY_SERVER_SLAVE_ROLE = 0,
    VALKEY_SERVER_MASTER_ROLE = 1,
    VALKEY_SERVER_TBD_ROLE = 2
};

enum VALKEY_SERVER_LOCATION_TYPE {
    VALKEY_SERVER_LOCATION_HOST_TYPE,
    VALKEY_SERVER_LOCATION_SOCKET_TYPE
};

typedef struct valkey_server {
    // Object marker.
#define VALKEY_SERVER_MAGIC 0xac587b11
    unsigned magic;

    // Database.
    struct vmod_valkey_db *db;

    // Location (allocated in the heap).
    struct {
        const char *raw;
        enum VALKEY_SERVER_LOCATION_TYPE type;
        union {
            struct {
                const char *host;
                unsigned port;
            } address;
            const char *path;
        } parsed;
    } location;

    // Role (rw field to be protected by db->mutex).
    enum VALKEY_SERVER_ROLE role;

    // Weight.
    unsigned weight;

    // Shared pool.
    struct {
        // Condition variable.
        pthread_cond_t cond;

        // Contexts (rw fields -allocated in the heap- to be protected by
        // db->mutex and the associated condition variable).
        unsigned ncontexts;
        VTAILQ_HEAD(,valkey_context) free_contexts;
        VTAILQ_HEAD(,valkey_context) busy_contexts;
    } pool;

    // Valkey Cluster state (rw fields to be protected by db->mutex).
    struct {
        unsigned slots[NVALKEY_CLUSTER_SLOTS];
    } cluster;

    // Sickness timestamps (rw fields to be protected by db->mutex): last time
    // the server was flagged as sick, and expiration of the last sickness
    // condition.
    struct {
        time_t tst;
        time_t exp;
    } sickness;

    // Tail queue.
    VTAILQ_ENTRY(valkey_server) list;
} valkey_server_t;

typedef struct valkey_context {
    // Object marker.
#define VALKEY_CONTEXT_MAGIC 0xe11eaa70
    unsigned magic;

    // Server.
    valkey_server_t *server;

    // Data (allocated in the heap).
    valkeyContext *rcontext;
    unsigned version;
    time_t tst;

    // Tail queue.
    VTAILQ_ENTRY(valkey_context) list;
} valkey_context_t;

struct vcl_state;
typedef struct vcl_state vcl_state_t;

struct vmod_valkey_db {
    // Object marker.
    unsigned magic;
#define VMOD_VALKEY_DATABASE_MAGIC 0xef35182b

    // Mutex.
    struct lock mutex;

    // Configuration.
    // XXX: required because PRIV_VCL pointers are not available when the
    // VMOD releases database instances. This should be fixed in future
    // Varnish releases.
    vcl_state_t *config;

    // General options (allocated in the heap).
    const char *name;
    struct timeval connection_timeout;
    unsigned connection_ttl;
    struct timeval command_timeout;
    unsigned max_command_retries;
    unsigned shared_connections;
    unsigned max_connections;
    enum VALKEY_PROTOCOL protocol;
#ifdef TLS_ENABLED
    valkeyTLSContext *tls_ctx;
#endif
    const char *user;
    const char *password;
    time_t sickness_ttl;
    unsigned ignore_slaves;

    // Valkey servers (rw field -allocated in the heap- to be protected by the
    // associated mutex), clustered by weight & role.
    VTAILQ_HEAD(,valkey_server) servers[NVALKEY_SERVER_WEIGHTS][NVALKEY_SERVER_ROLES];

    // Valkey Cluster options.
    struct {
        unsigned enabled;
        unsigned max_hops;
    } cluster;

    // Stats (rw fields to be protected by the associated mutex).
    struct stats {
        struct {
            // Number of successfully created servers.
            uint64_t total;
            // Number of failures while trying to create new servers.
            uint64_t failed;
        } servers;

        struct {
            // Number of successfully created connections.
            uint64_t total;
            // Number of failures while trying to create new connections.
            uint64_t failed;
            // Number of (established and probably healthy) connections dropped.
            struct {
                uint64_t error;
                uint64_t hung_up;
                uint64_t overflow;
                uint64_t ttl;
                uint64_t version;
                uint64_t sick;
            } dropped;
        } connections;

        struct {
            // Number of times some worker thread have been blocked waiting for
            // a free connection.
            uint64_t blocked;
        } workers;

        struct {
            // Number of successfully executed commands (this includes
            // error replies).
            uint64_t total;
            // Number of failed command executions (this does not include
            // error replies). If retries have been requested, each failed try
            // is considered as a separate command.
            uint64_t failed;
            // Number of retried command executions (this includes both
            // successful and failed executions).
            uint64_t retried;
            // Number of successfully executed commands returning a Valkey error
            // reply.
            uint64_t error;
            // Number of NOSCRIPT error replies while executing EVALSHA
            // commands.
            uint64_t noscript;
        } commands;

        struct {
            struct {
                // Number of successfully executed discoveries.
                uint64_t total;
                // Number of failed discoveries (this includes connection
                // failures, unexpected responses, etc.).
                uint64_t failed;
            } discoveries;
            struct {
                // Number of MOVED replies.
                uint64_t moved;
                // Number of ASK replies.
                uint64_t ask;
            } replies;
        } cluster;
    } stats;
};

typedef struct task_state {
    // Object marker.
#define TASK_STATE_MAGIC 0xa6bc103e
    unsigned magic;

    // Private contexts (allocated in the heap).
    unsigned ncontexts;
    VTAILQ_HEAD(,valkey_context) contexts;

    // Current database.
    struct vmod_valkey_db *db;

    // Valkey command:
    //   - Database.
    //   - Arguments (allocated in the session workspace).
    //   - Reply (allocated in the heap).
#define MAX_VALKEY_COMMAND_ARGS 128
    struct {
        struct vmod_valkey_db *db;
        struct timeval timeout;
        unsigned max_retries;
        unsigned argc;
        const char *argv[MAX_VALKEY_COMMAND_ARGS];
        valkeyReply *reply;
    } command;
} task_state_t;

typedef struct subnet {
    // Object marker.
#define SUBNET_MAGIC 0x27facd57
    unsigned magic;

    // Weight.
    unsigned weight;

    // Address and mask stored in unsigned 32 bit variables (in_addr.s_addr)
    // using host byte oder.
    // XXX: only IPv4 subnets supported.
    struct in_addr address;
    struct in_addr mask;

    // Tail queue.
    VTAILQ_ENTRY(subnet) list;
} subnet_t;

typedef struct database {
    // Object marker.
#define DATABASE_MAGIC 0x9200fda1
    unsigned magic;

    // Database.
    struct vmod_valkey_db *db;

    // Tail queue.
    VTAILQ_ENTRY(database) list;
} database_t;

struct vcl_state {
    // Object marker.
#define VCL_STATE_MAGIC 0x77feec11
    unsigned magic;

    // Mutex.
    struct lock mutex;

    // Subnets (rw field to be protected by the associated mutex).
    VTAILQ_HEAD(,subnet) subnets;

    // Databases (rw field to be protected by the associated mutex).
    VTAILQ_HEAD(,database) dbs;

    // Sentinel (rw fields to be protected by the associated mutex).
    struct {
        // Raw configuration.
        const char *locations;
        unsigned period;
        struct timeval connection_timeout;
        struct timeval command_timeout;
        enum VALKEY_PROTOCOL protocol;
#ifdef TLS_ENABLED
        unsigned tls;
        const char *tls_cafile;
        const char *tls_capath;
        const char *tls_certfile;
        const char *tls_keyfile;
        const char *tls_sni;
#endif
        const char *password;

        // Thread reference + shared state.
        pthread_t thread;
        unsigned active;
        unsigned discovery;
    } sentinels;
};

typedef struct vmod_state {
    // Mutex.
    pthread_mutex_t mutex;

    // Version increased on every VCL warm event (rw field protected by the
    // associated mutex on writes; it's ok to ignore the lock during reads).
    // This will be used to (1) reestablish connections binded to worker
    // threads; and (2) regenerate pooled connections shared between threads.
    unsigned version;

    // Varnish locks.
    struct {
        unsigned refs;
        struct vsc_seg *vsc_seg;
        struct VSC_lck *config;
        struct VSC_lck *db;
    } locks;
} vmod_state_t;

extern vmod_state_t vmod_state;

// See: https://stackoverflow.com/a/8814003/1806102.
#define VALKEY_ERRSTR_1(rcontext) \
    (rcontext->err ? rcontext->errstr : "-")
#define VALKEY_ERRSTR_2(rcontext, reply) \
    (rcontext->err ? \
     rcontext->errstr : \
     ((reply != NULL && \
       (reply->type == VALKEY_REPLY_ERROR || \
        reply->type == VALKEY_REPLY_STATUS || \
        reply->type == VALKEY_REPLY_STRING)) ? reply->str : "-"))
#define VALKEY_ERRSTR_X(x, rcontext, reply, FUNC, ...)  FUNC
#define VALKEY_ERRSTR(...) \
    VALKEY_ERRSTR_X(,##__VA_ARGS__, \
        VALKEY_ERRSTR_2(__VA_ARGS__), \
        VALKEY_ERRSTR_1(__VA_ARGS__))

#if LIBVALKEY_MAJOR >= 0 && LIBVALKEY_MINOR >= 1
#define RESP3_ENABLED 1
#define RESP3_SWITCH(a, b) a
#else
#define RESP3_SWITCH(a, b) b
#endif

#define VALKEY_LOG(ctx, priority, fmt, ...) \
    do { \
        const struct vrt_ctx *_ctx = ctx; \
        \
        char *_buffer; \
        if (priority <= LOG_ERR) { \
            assert(asprintf( \
                &_buffer, \
                "[VALKEY][%s:%d] %s", __func__, __LINE__, fmt) > 0); \
        } else { \
            assert(asprintf( \
                &_buffer, \
                "[VALKEY] %s", fmt) > 0); \
        } \
        \
        syslog(priority, _buffer, ##__VA_ARGS__); \
        \
        unsigned _tag; \
        if (priority <= LOG_ERR) { \
            _tag = SLT_VCL_Error; \
        } else { \
            _tag = SLT_VCL_Log; \
        } \
        if ((_ctx != NULL) && (_ctx->vsl != NULL)) { \
            VSLb(_ctx->vsl, _tag, _buffer, ##__VA_ARGS__); \
        } else { \
            VSL(_tag, 0, _buffer, ##__VA_ARGS__); \
        } \
        \
        free(_buffer); \
    } while (0)

#define VALKEY_LOG_ERROR(ctx, fmt, ...) \
    VALKEY_LOG(ctx, LOG_ERR, fmt, ##__VA_ARGS__)
#define VALKEY_LOG_WARNING(ctx, fmt, ...) \
    VALKEY_LOG(ctx, LOG_WARNING, fmt, ##__VA_ARGS__)
#define VALKEY_LOG_INFO(ctx, fmt, ...) \
    VALKEY_LOG(ctx, LOG_INFO, fmt, ##__VA_ARGS__)

#define VALKEY_FAIL(ctx, result, fmt, ...) \
    do { \
        syslog(LOG_ALERT, "[VALKEY][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
        VRT_fail(ctx, "[VALKEY][%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__); \
        return result; \
    } while (0)

#define VALKEY_FAIL_WS(ctx, result) \
    VALKEY_FAIL(ctx, result, "Workspace overflow")

#define VALKEY_FAIL_INSTANCE(ctx, result) \
    VALKEY_FAIL(ctx, result, "Failed to create instance")

#ifdef TLS_ENABLED
#define VALKEY_TLS(ctx, rcontext, db, message1, message2, ...) \
    do { \
        if (db->tls_ctx != NULL && \
            valkeyInitiateTLSWithContext(rcontext, db->tls_ctx) != VALKEY_OK) { \
            VALKEY_LOG_ERROR(ctx, \
                message1 " (error=%d, " message2 "): %s", \
                rcontext->err, ##__VA_ARGS__, VALKEY_ERRSTR(rcontext)); \
            valkeyFree(rcontext); \
            rcontext = NULL; \
        } \
    } while (0)
#else
#define VALKEY_TLS(ctx, rcontext, db, message1, message2, ...)
#endif

#define VALKEY_BLESS_CONTEXT(ctx, rcontext, db, message1, message2, ...) \
    do { \
        VALKEY_TLS(ctx, rcontext, db, message1, message2, ##__VA_ARGS__); \
        \
        if (rcontext != NULL) { \
            valkeyReply *reply = NULL; \
            \
            if (db->protocol == VALKEY_PROTOCOL_DEFAULT) { \
                if (db->password != NULL) { \
                    if (db->user != NULL) { \
                        reply = valkeyCommand(rcontext, "AUTH %s %s", db->user, db->password); \
                    } else { \
                        reply = valkeyCommand(rcontext, "AUTH %s", db->password); \
                    } \
                    \
                    if ((rcontext->err) || \
                        (reply == NULL) || \
                        (reply->type != VALKEY_REPLY_STATUS) || \
                        (strcmp(reply->str, "OK") != 0)) { \
                        VALKEY_LOG_ERROR(ctx, \
                            message1 " (error=%d, " message2 "): %s", \
                            rcontext->err, ##__VA_ARGS__, VALKEY_ERRSTR(rcontext, reply)); \
                        valkeyFree(rcontext); \
                        rcontext = NULL; \
                    } \
                } \
            } else { \
                if (db->password != NULL) { \
                    reply = valkeyCommand(rcontext, "HELLO %d AUTH %s %s", \
                        db->protocol, (db->user != NULL) ? db->user : "default", db->password); \
                } else { \
                    reply = valkeyCommand(rcontext, "HELLO %d", db->protocol); \
                } \
                \
                if ((rcontext->err) || \
                    (reply == NULL) || \
                    (reply->type != VALKEY_REPLY_ARRAY && \
                     RESP3_SWITCH(reply->type != VALKEY_REPLY_MAP, 1)) \
                   ) { \
                    VALKEY_LOG_ERROR(ctx, \
                        message1 " (error=%d, " message2 "): %s", \
                        rcontext->err, ##__VA_ARGS__, VALKEY_ERRSTR(rcontext, reply)); \
                    valkeyFree(rcontext); \
                    rcontext = NULL; \
                } \
            } \
            \
            if (reply != NULL) {  \
                freeReplyObject(reply);  \
            } \
        } \
    } while (0)

valkey_server_t *new_valkey_server(
    struct vmod_valkey_db *db, const char *location, enum VALKEY_SERVER_ROLE role);
void free_valkey_server(valkey_server_t *server);

valkey_context_t *new_valkey_context(
    valkey_server_t *server, valkeyContext *rcontext, time_t tst);
void free_valkey_context(valkey_context_t *context);

struct vmod_valkey_db *new_vmod_valkey_db(
    vcl_state_t *config, const char *name, struct timeval connection_timeout,
    unsigned connection_ttl, struct timeval command_timeout, unsigned max_command_retries,
    unsigned shared_connections, unsigned max_connections, enum VALKEY_PROTOCOL protocol,
#ifdef TLS_ENABLED
    valkeyTLSContext *tls_ctx,
#endif
    const char *user, const char *password, unsigned sickness_ttl,
    unsigned ignore_slaves, unsigned clustered, unsigned max_cluster_hops);
void free_vmod_valkey_db(struct vmod_valkey_db *db);

task_state_t *new_task_state();
void free_task_state(task_state_t *state);

vcl_state_t *new_vcl_state();
void free_vcl_state(vcl_state_t *priv);

subnet_t *new_subnet(unsigned weight, struct in_addr ia4, unsigned bits);
void free_subnet(subnet_t *subnet);

database_t *new_database(struct vmod_valkey_db *db);
void free_database(database_t *db);

valkeyReply *valkey_execute(
    VRT_CTX, struct vmod_valkey_db *db, task_state_t *state, struct timeval timeout,
    unsigned max_retries, unsigned argc, const char *argv[], unsigned *retries,
    valkey_server_t *server, unsigned asking, unsigned master, unsigned slot);

valkey_server_t * unsafe_add_valkey_server(
    VRT_CTX, struct vmod_valkey_db *db, vcl_state_t *config,
    const char *location, enum VALKEY_SERVER_ROLE role);

#endif
