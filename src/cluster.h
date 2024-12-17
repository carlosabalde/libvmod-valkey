#ifndef CLUSTER_H_INCLUDED
#define CLUSTER_H_INCLUDED

#include <valkey/valkey.h>

#include "core.h"

void discover_cluster_slots(
    VRT_CTX, struct vmod_valkey_db *db, vcl_state_t *config, valkey_server_t *server);

valkeyReply *cluster_execute(
    VRT_CTX, struct vmod_valkey_db *db, vcl_state_t *config, task_state_t *state,
    struct timeval timeout, unsigned max_retries, unsigned argc, const char *argv[],
    unsigned *retries, unsigned master);

#endif
