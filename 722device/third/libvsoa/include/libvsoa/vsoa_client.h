/*
 * Copyright (c) 2021 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_client.h Vehicle SOA client.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_CLIENT_H
#define VSOA_CLIENT_H

#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "vsoa_parser.h"

/* Client timer period (ms) */
#define VSOA_CLIENT_TIMER_PERIOD  10

/* Client default timeout (ms) */
#define VSOA_CLIENT_DEF_TIMEOUT  60000

/* Client default send timeout (ms) */
#define VSOA_CLIENT_DEF_SEND_TIMEOUT  500

/* VSOA stream keepalive timeout seconds */
#define VSOA_CLIENT_KEEPALIVE_TIMEOUT  10

/* Client RPC method */
#define VSOA_CLIENT_RPC_METHOD_GET  0
#define VSOA_CLIENT_RPC_METHOD_SET  1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTICE: The following functions are Thread-Safe:
 *         vsoa_client_ping(),
 *         vsoa_client_subscribe(),
 *         vsoa_client_unsubscribe(),
 *         vsoa_client_multi_subscribe(),
 *         vsoa_client_multi_unsubscribe(),
 *         vsoa_client_call(),
 *         vsoa_client_sync_create(),
 *         vsoa_client_sync_delete(),
 *         vsoa_client_sync_call(),
 *         vsoa_client_datagram(),
 *         vsoa_client_quick_datagram(),
 *         vsoa_client_is_connect(),
 *         vsoa_client_send_timeout(),
 *         vsoa_client_set_custom(),
 *         vsoa_client_custom(),
 *         vsoa_client_stream_create(),
 *         vsoa_client_stream_close().
 *
 *         All callback functions are all called in the vsoa_client_input_fds(),
 *         so you need to pay attention to multi-thread safety.
 *
 *         When the connection is disconnected, you only need to re-call vsoa_client_connect()
 *         to reconnect. After reconnecting, you need to re-subscribe to the previous topics.
 *         vsoa_client_connect() reconnecting can only be called in the event loop where
 *         vsoa_client_input_fds() is located. not in various callbacks.
 *
 *         After vsoa_client_close() is called, the client object will no longer be available.
 *         vsoa_client_close() can only be called in the event loop where vsoa_client_input_fds()
 *         is located, not in various callbacks.
 *
 *         The quick channel is used for high-frequency data update channels.
 *         Due to the high data update frequency, the requirements for communication reliability
 *         are not strict.
 */

/*
 * URL path rules:
 *
 * Path: "/"        Sub-Pub: Catch all publish message.
 *
 * Path: "/a/b/c"   Sub-Pub: Only catch "/a/b/c" publish message.
 *
 * Path: "/a/b/c/"  Sub-Pub: Catch "/a/b/c" and "/a/b/c/..." all publish message.
 */

/* Structure declaration */
struct vsoa_client;

/* Client type */
typedef struct vsoa_client vsoa_client_t;

/* Client on message callback (received subscribed messages) */
typedef void (*vsoa_client_msg_func_t)(void *arg, vsoa_client_t *client, vsoa_url_t *url, vsoa_payload_t *payload, bool quick);

/* Client RPC callback (`vsoa_hdr` NULL means server not responding)
 * The memory pointed to by `vsoa_hdr` and `payload` will be invalidated when the callback function returns */
typedef void (*vsoa_client_rpc_func_t)(void *arg, vsoa_client_t *client, vsoa_header_t *vsoa_hdr, vsoa_payload_t *payload);

/* Client subscribe, unsubscribe and ping callback */
typedef void (*vsoa_client_res_func_t)(void *arg, vsoa_client_t *client, bool success);

/* Client on datagram callback same as on message */
typedef void (*vsoa_client_dat_func_t)(void *arg, vsoa_client_t *client, vsoa_url_t *url, vsoa_payload_t *payload, bool quick);

/* Structure of client sync call */
struct vsoa_client_sync_call;

/* Client sync call type */
typedef struct vsoa_client_sync_call vsoa_client_sync_call_t;

/* Create VSOA client 
 * Warning: This function must be mutually exclusive with the vsoa_client_close() call */
vsoa_client_t *vsoa_client_create(vsoa_client_msg_func_t onmsg, void *arg);

/* Close VSOA client 
 * Warning: This function must be mutually exclusive with the vsoa_client_create() call */
void vsoa_client_close(vsoa_client_t *client);

/* Set VSOA client socket linger time
 * This function can be called after each successful connect, the time unit is seconds,
 * when the time is 0, it means that the close or disconnect call will be closed immediately */
bool vsoa_client_linger(vsoa_client_t *client, int time);

/* Connect to server (Synchronous) */
bool vsoa_client_connect(vsoa_client_t *client, const struct sockaddr *server, socklen_t namelen,
                         const struct timespec *timeout, const char *passwd, char *info, size_t sz_info);

/* Disconnect from server
 * After disconnect, the `vsoa_client_connect` function can be called again */
bool vsoa_client_disconnect(vsoa_client_t *client);

/* VSOA client is connect with server */
bool vsoa_client_is_connect(vsoa_client_t *client);

/* VSOA client path separation */
char *vsoa_client_path_token(vsoa_client_t *client, vsoa_url_t *url, size_t *len);

/* VSOA client send timeout
 * `timeout` NULL means use VSOA_CLIENT_DEF_SEND_TIMEOUT */
bool vsoa_client_send_timeout(vsoa_client_t *client, const struct timespec *timeout);

/* VSOA client checking event */
int vsoa_client_fds(vsoa_client_t *client, fd_set *rfds);

/* VSOA client input event */
bool vsoa_client_input_fds(vsoa_client_t *client, const fd_set *rfds);

/* Ping server */
bool vsoa_client_ping(vsoa_client_t *client, vsoa_client_res_func_t callback, void *arg, const struct timespec *timeout);

/* Subscribe URL */
bool vsoa_client_subscribe(vsoa_client_t *client, const vsoa_url_t *url,
                           vsoa_client_res_func_t callback, void *arg, const struct timespec *timeout);

/* Unsubscribe URL */
bool vsoa_client_unsubscribe(vsoa_client_t *client, const vsoa_url_t *url,
                             vsoa_client_res_func_t callback, void *arg, const struct timespec *timeout);

/* Subscribe Multi URL */
bool vsoa_client_multi_subscribe(vsoa_client_t *client, char * const urls[], int cnt,
                                 vsoa_client_res_func_t callback, void *arg, const struct timespec *timeout);

/* Unsubscribe Multi URL */
bool vsoa_client_multi_unsubscribe(vsoa_client_t *client, char * const urls[], int cnt,
                                   vsoa_client_res_func_t callback, void *arg, const struct timespec *timeout);

/* RPC call
 * This function is an asynchronous RPC call.
 * If you need a synchronous RPC call,
 * you can use RPC call synchronization extension interface. */
bool vsoa_client_call(vsoa_client_t *client, int method, const vsoa_url_t *url, const vsoa_payload_t *payload,
                      vsoa_client_rpc_func_t callback, void *arg, const struct timespec *timeout);

/* Send datagram to server */
bool vsoa_client_datagram(vsoa_client_t *client, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* Send datagram to server use quick channel */
bool vsoa_client_quick_datagram(vsoa_client_t *client, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* VSOA client set on datagram callback */
void vsoa_client_on_datagram(vsoa_client_t *client, vsoa_client_dat_func_t callback, void *arg);

/* VSOA client set custom data */
void vsoa_client_set_custom(vsoa_client_t *client, void *custom);

/* VSOA client get custom data */
void *vsoa_client_custom(vsoa_client_t *client);

/* VSOA client stream create and return a stream file descriptor (Synchronous)
 * A stream is a new TCP connection that you can use to transfer data to and from the server */
int vsoa_client_stream_create(vsoa_client_t *client, uint16_t tunid, const struct timespec *timeout, int keepalive);

/* VSOA client stream close 
 * After the stream is closed, the stream file descriptor is not allowed to be used again */
void vsoa_client_stream_close(int stream);

/* RPC call synchronization extension interface
 * VSOA client create a RPC call synchronizer,
 * A synchronizer can only process one RPC synchronization call at the same time,
 * and multi-threading is not allowed.
 *
 * If `dynamic` is true, it means that the package memory is dynamically applied for each time the call receives a reply from the server.
 * When it is false, it means that it is only applied statically once.
 *
 * It is recommended to create a synchronizer for each thread that will use synchronous RPC calls,
 * and then use it sequentially in the corresponding thread.
 */
vsoa_client_sync_call_t *vsoa_client_sync_create(bool dynamic);

/* VSOA client destroy an RPC call synchronizer
 * The synchronizer can only be destroyed after the use is completed,
 * and is not allowed to be destroyed during the call.
 */
bool vsoa_client_sync_delete(vsoa_client_sync_call_t *sync);

/* The VSOA client performs a synchronous RPC call, (Synchronous)
 * this function will block until the call completes, an error occurs, or it times out,
 * so this function cannot be used in the VSOA main event loop thread.
 *
 * Return true means the call is successful,
 * `vsoa_hdr_reply` is valid means the server has replied (otherwise means the command timed out),
 * when the call is successful, you can check `vsoa_hdr_reply` to get the server reply,
 * you can use `vsoa_parser_get_payload()` to get the reply payload.
 *
 * The packet memory pointed to by `vsoa_hdr_reply` will be invalid when the next remote call is made or synchronizer is deleted.
 */
bool vsoa_client_sync_call(vsoa_client_t *client, int method, const vsoa_url_t *url, const vsoa_payload_t *payload,
                           vsoa_client_sync_call_t *sync, vsoa_header_t **vsoa_hdr_reply, const struct timespec *timeout);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_CLIENT_H */
/*
 * end
 */
