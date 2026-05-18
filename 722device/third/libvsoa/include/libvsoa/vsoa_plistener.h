/*
 * Copyright (c) 2023 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_plistener.h Vehicle SOA parallel RPC listener.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_PLISTENER_H
#define VSOA_PLISTENER_H

#include "vsoa_server.h"

/* VSOA Max parallel RPC worker threads */
#define VSOA_SERVER_MAX_RPC_WORKER_THDS  8

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_plistener;

/* VSOA parallel RPC listener */
typedef struct vsoa_plistener   vsoa_plistener_t;

/* VSOA parallel RPC handler */
typedef struct {
    vsoa_server_cmd_func_t callback;
    void *arg;
} vsoa_plistener_handler_t;

/* VSOA Create parallel RPC listener */
vsoa_plistener_t *vsoa_server_plistener_create(int thd_cnt);

/* VSOA Delete parallel RPC listener
 * Warning: All `vsoa_plistener_handler_t` of this `plistener` can no longer be used,
 * please make sure all `vsoa_plistener_handler_t` has been called `vsoa_server_remove_listener` */
void vsoa_server_plistener_delete(vsoa_plistener_t *plistener);

/* VSOA create a parallel RPC listener
 * If the `callback` member of the returned `vsoa_plistener_handler_t` structure is NULL, it means that the creation failed
 * If `parallel` is true, it means that this `callback` can be executed in parallel, 
 * and if it is false, it means that this `callback` does not support parallelism.
 * Developers need to handle the `callback` thread safety that can be executed in parallel mode
 * `max_queued` indicates that if there is processing congestion in this command, 
 * what is the max queueing length, if the length is exceeded, this command will blocked until worker thread idle.
 * Zero means infinite max queued length */
vsoa_plistener_handler_t vsoa_server_plistener_handler(vsoa_plistener_t *plistener,
                                                       bool parallel, int max_queued,
                                                       vsoa_server_cmd_func_t callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_PLISTENER_H */
/*
 * end
 */
