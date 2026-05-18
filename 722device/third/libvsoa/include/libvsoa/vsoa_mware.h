/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_mware.h Vehicle SOA server middleware framework.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_MWARE_H
#define VSOA_MWARE_H

#include "vsoa_server.h"

/* This module provides the VSOA server middleware framework, 
 * which can quickly implement chained command processing */

#ifdef __cplusplus
extern "C" {
#endif

/* Middleware resolve data declaration */
struct vsoa_mware_resolve;

/* Middleware resolve data type */
typedef struct vsoa_mware_resolve vsoa_mware_resolve_t;

/* Middleware resolve data free callback */
typedef void (*vsoa_mware_resolve_free_func_t)(void *value, const char *key, size_t value_len);

/* Middleware add resolve data (`key` will be referenced by `vsoa_mware_resolve_t`)
 * The middleware callback can add the resolved data to `vsoa_mware_resolve_t` and pass it to next */
bool vsoa_mware_add_resolve_data(vsoa_mware_resolve_t *resolve,
                                 const char *key, void *value, vsoa_mware_resolve_free_func_t free);

/* Find a previously added data of the specified key */
void *vsoa_mware_get_resolve_data(vsoa_mware_resolve_t *resolve, const char *key);

/* Middleware add resolve data with value length */
bool vsoa_mware_add_resolve_data_with_length(vsoa_mware_resolve_t *resolve,
                                             const char *key, void *value, size_t value_len, vsoa_mware_resolve_free_func_t free);

/* Find a previously added data of the specified key with value length */
void *vsoa_mware_get_resolve_data_with_length(vsoa_mware_resolve_t *resolve, const char *key, size_t *value_len);

/* Middleware declaration */
struct vsoa_mware;

/* Middleware type */
typedef struct vsoa_mware vsoa_mware_t;

/* Create middleware */
vsoa_mware_t *vsoa_mware_create(void);

/* Delete middleware
 * Must ensure that the server no longer uses this middleware main callback */
void vsoa_mware_delete(vsoa_mware_t *mware);

/* Get middleware main command callback
 * `arg` must be `mware` when calling `vsoa_server_add_listener`
 * NOTICE: When using `vsoa_server_plistener_handler()` to add a middleware handler, the `parallel` parameter must be `false`. */
vsoa_server_cmd_func_t vsoa_mware_handler(vsoa_mware_t *mware);

/* Sub command callback
 * NOTICE: `vsoa_hdr`, `url` and `payload` are invalid when this function returns */
typedef bool (*vsoa_mware_cmd_func_t)(void *arg, vsoa_server_t *server, vsoa_cli_id_t id,
                                      vsoa_header_t *vsoa_hdr, vsoa_url_t *url, vsoa_payload_t *payload,
                                      vsoa_mware_resolve_t *resolve);

/* Middleware add RPC listener (execute in order of addition) */
bool vsoa_mware_add_listener(vsoa_mware_t *mware, vsoa_mware_cmd_func_t callback, void *arg);

/* Middleware remove RPC listener */
void vsoa_mware_remove_listener(vsoa_mware_t *mware, vsoa_mware_cmd_func_t callback, void *arg);

/* Middleware set custom data */
void vsoa_mware_set_custom(vsoa_mware_t *mware, void *custom);

/* Middleware get custom data */
void *vsoa_mware_custom(vsoa_mware_t *mware);

/* Middleware resolve to middleware */
vsoa_mware_t *vsoa_mware_get(vsoa_mware_resolve_t *resolve);

/* Sometimes when processing a command, we need to cooperate with other asynchronous data. 
 * For example, we may need to call another client's RPC and synthesize the results to reply. 
 * In this case, we need to resolve data to manually handle the life cycle. 
 * The following functions support resolve data references. */

/* Resolve data reference
 * This function returns the number of references to `resolve`. Returning negative number is an error.
 * Normally, resolve data will be automatically released when the command callback chain ends.
 * If an asynchronous callback is used in the user command callback list, 
 * resolve data can be retained through the `vsoa_mware_ref_resolve()` operation until it is released at `vsoa_mware_unref_resolve()`.
 * NOTICE: That `vsoa_mware_ref_resolve()` and `vsoa_mware_unref_resolve()` must appear in pairs, otherwise a memory leak will occur */
int vsoa_mware_ref_resolve(vsoa_mware_resolve_t *resolve);

/* Resolve data unreference
 * This function returns the number of references to `resolve`. Returning negative number is an error.
 * When this function returns 0, `resolve` is no longer available.
 * NOTICE: All referenced resolve data must be unreference before calling `vsoa_mware_delete()` */
int vsoa_mware_unref_resolve(vsoa_mware_resolve_t *resolve);

/* Reply to the caller with the RPC information saved when using `vsoa_mware_ref_resolve()` */
bool vsoa_mware_reply_resolve(vsoa_mware_resolve_t *resolve, uint8_t status, uint16_t tunid, const vsoa_payload_t *payload);

/* Automatically create middleware and quickly initialize server commands listeners
 * example:
 * vsoa_mware_listener_t cmds[] = {
 *     { first_step,  first_arg  },
 *     { second_step, second_arg },
 *     { third_step,  third_arg  }
 * };
 * vsoa_url_t url = { "/test", 5 };
 * vsoa_mware_t *mware = vsoa_mware_add_server_listener(server, &url, cmds, 3);
 * ...
 * exit:
 * vsoa_mware_remove_server_listener(mware);
 */

/* command listener processing queue: */
typedef struct {
    vsoa_mware_cmd_func_t listener;
    void *arg;
} vsoa_mware_listener_t;

/* Automatically create middleware and add RPC listeners queue (synchronize call queue) */
vsoa_mware_t *vsoa_mware_add_server_listener(vsoa_server_t *server, const vsoa_url_t *url, vsoa_mware_listener_t cmds[], int cmd_cnt);

/* Delete automatically middleware and delete RPC listeners queue */
void vsoa_mware_remove_server_listener(vsoa_mware_t *mware);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_MWARE_H */
/*
 * end
 */
