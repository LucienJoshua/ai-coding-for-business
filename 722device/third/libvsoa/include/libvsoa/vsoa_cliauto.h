/*
 * Copyright (c) 2023 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_cliauto.h Vehicle SOA client auto robot.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_CLIAUTO_H
#define VSOA_CLIAUTO_H

#include "vsoa_client.h"

/* This module is a VSOA client extension module that provides automatic client connection, 
 * automatic handling of broken links, automatic subscribe to events, and automatic processing of event loops. 
 * Using this robot module can reduce the size of client code, 
 * and developers only need to focus on the business itself */

/* Warning: Except `vsoa_client_auto_handle` other functions are not allowed to be called in this client
 * event loop thread context is not allowed to be called in the client event loop,
 * such as asynchronous RPC callback, or subscription message callback, etc. */

/* Max server info length */
#define VSOA_CLIENT_AUTO_MAX_INFO  2048

/* Max ping lost count */
#define VSOA_CLIENT_AUTO_MAX_PING_LOST  3

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_client_auto;

/* Client auto type */
typedef struct vsoa_client_auto vsoa_client_auto_t;

/* Client on connect callback 
 * If `connect` is true, it means the connection is successful and the subscription is successful.
 * At this time, the robot has performed a subscription operation and has not yet performed data consistency operations.
 * If `connect` is false, it means the link is disconnected. When the link is disconnected, 
 * the client auto module will automatically reconnect.
 * When the connection is successful, `info` indicates the server information */
typedef void (*vsoa_client_conn_func_t)(void *arg, vsoa_client_auto_t *cliauto, bool connect, const char *info);

/* VSOA client auto create 
 * The callback function will be called in the client auto thread context */
vsoa_client_auto_t *vsoa_client_auto_create(vsoa_client_msg_func_t onmsg, void *arg);

/* VSOA client auto delete
 * After this function is called, the `cliauto` object becomes invalid and is not allowed to be used again */
void vsoa_client_auto_delete(vsoa_client_auto_t *cliauto);

/* VSOA client auto setup
 * The callback function will be called in the client auto thread context */
bool vsoa_client_auto_setup(vsoa_client_auto_t *cliauto, vsoa_client_conn_func_t onconn, void *arg);

/* VSOA client auto data consistent
 * When VSOA disconnected and reconnected, the server data changes, which we cannot detect, 
 * so this function can set the URL collection of interest. 
 * The robot will use the RPC call with GET method to continuously obtain these data, 
 * and then **simulate** the server PUBLISH operation. Update the corresponding data 
 * The `urls` specified by this function will be RPC called when the connection is successful. 
 * NOTICE: 1: Ensure that the data published by the server on the same URL is consistent with the RPC GET data.
 *         2: Rapidly updated publish data should not use this method.
 *         3: This function must be called before `vsoa_client_auto_start` */
bool vsoa_client_auto_consistent(vsoa_client_auto_t *cliauto, char *const urls[], int url_cnt, unsigned int rpc_timeout);

/* VSOA client ping turbo (experimental)
 * When there is an RPC call pending, and if there is data packet loss,
 * and the client and server need to do their best to perform TCP fast retransmission at this time,
 * you can set this `turbo` parameter, the minimum value is 25ms. 0 means disable turbo ping.
 * When turbo ping is enabled, the `turbo` value must be less than or equal to `keepalive` in `vsoa_client_auto_start`
 * `max_cnt`: The maximum number of consecutive bursts must be greater than or equal to 3 */
bool vsoa_client_auto_ping_turbo(vsoa_client_auto_t *cliauto, unsigned int turbo, unsigned int max_cnt);

/* VSOA client auto start
 * `server` can be ip:port or service hostname, client auto will automatically use position to query hostname address.
 * `urls` and `cnt`: URLs to subscribe to when the link is established.
 * `keepalive`: How often (millisecond) to perform a ping operation, the minimum is 50ms, and the ping timeout is same as this value.
 * `conn_timeout`: Connection timeout, unit: millisecond, the minimum is 20ms.
 * `reconn_delay`: When the connection times out or the password is incorrect or the network is disconnected, 
 *                 the retry waiting time, unit: millisecond, the minimum is 20ms.
 * If turbo ping is enable, `keepalive` is recommended to be an integer multiple of `turbo`. */
bool vsoa_client_auto_start(vsoa_client_auto_t *cliauto, const char *server, const char *passwd, 
                            char * const urls[], int url_cnt, 
                            unsigned int keepalive, unsigned int conn_timeout, unsigned int reconn_delay);

/* VSOA client auto stop
 * `vsoa_client_auto_start` and `vsoa_client_auto_stop` must be called sequentially */
bool vsoa_client_auto_stop(vsoa_client_auto_t *cliauto);

/* VSOA client auto get client handle 
 * This handle is only used for communication, 
 * and cannot perform state operations such as connection closing, 
 * otherwise it will destroy the client auto logic.
 * This client handle is valid after `vsoa_client_auto_create` and invalid after `vsoa_client_auto_delete` */
vsoa_client_t *vsoa_client_auto_handle(vsoa_client_auto_t *cliauto);

/* VSOA client auto get server address
 * This function is recommended to be called in connect callback. */
bool vsoa_client_auto_server_address(vsoa_client_auto_t *cliauto, struct sockaddr *addr, socklen_t *namelen);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_CLIAUTO_H */
/*
 * end
 */
