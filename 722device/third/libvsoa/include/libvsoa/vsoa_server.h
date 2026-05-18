/*
 * Copyright (c) 2021 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_server.h Vehicle SOA server.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_SERVER_H
#define VSOA_SERVER_H

#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "vsoa_parser.h"

/* VSOA server backlog */
#define VSOA_SERVER_BACKLOG  32

/* VSOA server default send timeout (ms) */
#define VSOA_SERVER_DEF_SEND_TIMEOUT  100

/* VSOA server default handshake timeout (ms) */
#define VSOA_SERVER_DEF_HANDSHAKE_TIMEOUT  5000

/* VSOA stream keepalive timeout seconds */
#define VSOA_SERVER_KEEPALIVE_TIMEOUT  10

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTICE: After the server is successfully created,
 *         The following functions are Thread-Safe:
 *         vsoa_server_count(),
 *         vsoa_server_is_subscribed(),
 *         vsoa_server_publish(),
 *         vsoa_server_quick_publish(),
 *         vsoa_server_cli_close(),
 *         vsoa_server_cli_is_subscribed(),
 *         vsoa_server_cli_address(),
 *         vsoa_server_cli_reply(),
 *         vsoa_server_cli_priority(),
 *         vsoa_server_cli_keepalive(),
 *         vsoa_server_cli_array(),
 *         vsoa_server_cli_datagram(),
 *         vsoa_server_cli_quick_datagram(),
 *         vsoa_server_cli_send_timeout(),
 *         vsoa_server_cli_set_authed(),
 *         vsoa_server_cli_authed(),
 *         vsoa_server_set_custom(),
 *         vsoa_server_custom(),
 *         vsoa_server_stream_create(),
 *         vsoa_server_stream_accept(),
 *         vsoa_server_stream_close().
 *
 *         All callback functions are called in vsoa_server_input_fds().
 *         so you need to pay attention to multi-thread safety.
 *
 *         QoS functions includes:
 *         vsoa_server_cli_priority(),
 *         vsoa_server_cli_send_timeout().
 *
 *         When the physical link with the client is broken,
 *         if there is no data to send, the server cannot know that
 *         the client has been disconnected, it is recommended
 *         to use: vsoa_server_cli_keepalive() for client keepalive detection.
 *
 *         After vsoa_server_close() is called, the server object will no longer be available.
 *         vsoa_server_close() can only be called in the event loop where vsoa_server_input_fds()
 *         is located, not in various callbacks.
 *
 *         The quick channel is used for high-frequency data update channels.
 *         Due to the high data update frequency, the requirements for communication reliability
 *         are not strict.
 *
 *         vsoa_server_create() and vsoa_server_close() cannot be called in parallel.
 *         and developers need to perform mutually exclusive operations.
 */

/*
 * URL path rules:
 *
 * Path: "/"        Sub-Pub: Catch all publish message.
 *                  RPC    : Default URL listener. (Can add only one handler, the new handler will overwrite the old)
 *
 * Path: "/a/b/c"   Sub-Pub: Only catch "/a/b/c" publish message.
 *                  RPC    : Only handle "/a/b/c" path call.
 *
 * Path: "/a/b/c/"  Sub-Pub: Catch "/a/b/c" and "/a/b/c/..." all publish message.
 *                  RPC    : Handle "/a/b/c" and "/a/b/c/..." all path calls.
 *
 * NOTICE: If both "/a/b/c" and "/a/b/c/" RPC handler are present,
 *         When the client makes a "/a/b/c" RPC call, "/a/b/c" handler is matched before "/a/b/c/".
 */

/* Structure declaration */
struct vsoa_server;

/* Server type */
typedef struct vsoa_server vsoa_server_t;

/* Remote client ID */
typedef uint32_t  vsoa_cli_id_t;

/* Server on client connect or lost callback */
typedef void (*vsoa_server_cli_func_t)(void *arg, vsoa_server_t *server, vsoa_cli_id_t id, bool connect);

/* Server command callback
 * NOTICE: Can not remove listener in callback
 *         `vsoa_hdr`, `url` and `payload` are invalid when this function returns */
typedef void (*vsoa_server_cmd_func_t)(void *arg, vsoa_server_t *server, vsoa_cli_id_t id,
                                       vsoa_header_t *vsoa_hdr, vsoa_url_t *url, vsoa_payload_t *payload);

/* Server on datagram callback */
typedef void (*vsoa_server_dat_func_t)(void *arg, vsoa_server_t *server, vsoa_cli_id_t id,
                                       vsoa_url_t *url, vsoa_payload_t *payload, bool quick);

/* Server stream */
typedef struct vsoa_server_stream {
    bool valid;
    int listenfd;
    int clifd;
    uint16_t tunid;
    void *custom;
} vsoa_server_stream_t;

/* Create VSOA server 
 * Warning: This function must be mutually exclusive with the vsoa_server_close() call */
vsoa_server_t *vsoa_server_create(const char *info_json);

/* Close VSOA server
 * Warning: This function must be mutually exclusive with the vsoa_server_create() call */
void vsoa_server_close(vsoa_server_t *server);

/* Set VSOA server password */
bool vsoa_server_passwd(vsoa_server_t *server, const char *passwd);

/* Start VSOA server */
bool vsoa_server_start(vsoa_server_t *server, const struct sockaddr *addr, socklen_t namelen);

/* Get VSOA server address (must be called after `vsoa_server_start`) */
bool vsoa_server_address(vsoa_server_t *server, struct sockaddr *addr, socklen_t *namelen);

/* Bind VSOA server to specified network interface */
bool vsoa_server_bind_if(vsoa_server_t *server, const char *ifname);

/* VSOA server set on client callback */
void vsoa_server_on_cli(vsoa_server_t *server, vsoa_server_cli_func_t oncli, void *arg);

/* VSOA server remote clients count */
int vsoa_server_count(vsoa_server_t *server);

/* VSOA server set this server send packet to client timeout.
 * `timeout` NULL means use default: VSOA_SERVER_DEF_SEND_TIMEOUT */
bool vsoa_server_send_timeout(vsoa_server_t *server, bool cur_clis, const struct timespec *timeout);

/* VSOA server is subscribed */
bool vsoa_server_is_subscribed(vsoa_server_t *server, const vsoa_url_t *url);

/* VSOA server publish */
bool vsoa_server_publish(vsoa_server_t *server, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* VSOA server publish use quick channel */
bool vsoa_server_quick_publish(vsoa_server_t *server, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* VSOA server add RPC listener */
bool vsoa_server_add_listener(vsoa_server_t *server,
                              const vsoa_url_t *url, vsoa_server_cmd_func_t callback, void *arg);

/* VSOA server remove RPC listener */
void vsoa_server_remove_listener(vsoa_server_t *server, const vsoa_url_t *url);

/* VSOA server close a client */
bool vsoa_server_cli_close(vsoa_server_t *server, vsoa_cli_id_t id);

/* VSOA client set client socket linger time
 * the time unit is seconds, when the time is 0,
 * it means that the close call will be closed immediately */
bool vsoa_server_cli_linger(vsoa_server_t *server, vsoa_cli_id_t id, int time);

/* VSOA remote client is subscribed */
bool vsoa_server_cli_is_subscribed(vsoa_server_t *server, vsoa_cli_id_t id, const vsoa_url_t *url);

/* VSOA remote client address */
bool vsoa_server_cli_address(vsoa_server_t *server, vsoa_cli_id_t id, struct sockaddr *addr, socklen_t *namelen);

/* VSOA server RPC reply */
bool vsoa_server_cli_reply(vsoa_server_t *server, vsoa_cli_id_t id,
                           uint8_t status, uint32_t seqno, uint16_t tunid, const vsoa_payload_t *payload);

/* VSOA server set client priority (0 low ~ 5 high) default: 0 */
bool vsoa_server_cli_priority(vsoa_server_t *server, vsoa_cli_id_t id, int new_prio);

/* VSOA remote client keepalive */
bool vsoa_server_cli_keepalive(vsoa_server_t *server, vsoa_cli_id_t id, int keepalive);

/* VSOA server get remote client id array */
int vsoa_server_cli_array(vsoa_server_t *server, vsoa_cli_id_t ids[], int max_cnt);

/* VSOA server set send packet to specified client timeout.
 * `timeout` NULL means use server global send timeout setting */
bool vsoa_server_cli_send_timeout(vsoa_server_t *server, vsoa_cli_id_t id, const struct timespec *timeout);

/* VSOA server set client authorization status
 * Unauthorized clients cannot receive publish messages.
 * This function is generally called in the onclient callback for server-independent client authentication */
bool vsoa_server_cli_set_authed(vsoa_server_t *server, vsoa_cli_id_t id, bool authed);

/* VSOA server get client authorization status
 * Defaults to true for new clients authenticated by server password */
bool vsoa_server_cli_authed(vsoa_server_t *server, vsoa_cli_id_t id);

/* VSOA server set remote client custom data */
bool vsoa_server_cli_set_custom(vsoa_server_t *server, vsoa_cli_id_t id, void *custom);

/* VSOA server get remote client custom data */
void *vsoa_server_cli_custom(vsoa_server_t *server, vsoa_cli_id_t id);

/* VSOA server send datagram */
bool vsoa_server_cli_datagram(vsoa_server_t *server, vsoa_cli_id_t id, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* VSOA server send datagram use quick channel */
bool vsoa_server_cli_quick_datagram(vsoa_server_t *server, vsoa_cli_id_t id, const vsoa_url_t *url, const vsoa_payload_t *payload);

/* VSOA server set on datagram callback */
void vsoa_server_on_datagram(vsoa_server_t *server, vsoa_server_dat_func_t callback, void *arg);

/* VSOA server checking event */
int vsoa_server_fds(vsoa_server_t *server, fd_set *rfds);

/* VSOA server input event */
void vsoa_server_input_fds(vsoa_server_t *server, const fd_set *rfds);

/* VSOA server set custom data */
void vsoa_server_set_custom(vsoa_server_t *server, void *custom);

/* VSOA server get custom data */
void *vsoa_server_custom(vsoa_server_t *server);

/* VSOA server stream create
 * A stream is a new TCP connection that you can use to transfer data to and from clients */
vsoa_server_stream_t *vsoa_server_stream_create(vsoa_server_t *server);

/* VSOA server stream accept
 * Waiting for the client to connect,
 * you can use select readable to judge the client connection status, (use vsoa_server_stream_lfd() to get listen fd)
 * which is convenient to control the client stream connection timeout */
int vsoa_server_stream_accept(vsoa_server_stream_t *stream,
                              struct sockaddr *addr, socklen_t *namelen, int keepalive);

/* VSOA server stream close 
 * After the stream is closed, the stream object is not allowed to be used again */
void vsoa_server_stream_close(vsoa_server_stream_t *stream);

/* VSOA server stream listen fd */
#define vsoa_server_stream_lfd(stream)   ((stream)->listenfd)

/* VSOA server stream fd */
#define vsoa_server_stream_fd(stream)   ((stream)->clifd)

/* VSOA server stream tunid */
#define vsoa_server_stream_tunid(stream)   ((stream)->tunid)

/* VSOA server stream set custom data */
#define vsoa_server_stream_set_custom(stream, custom)   ((stream)->custom = (custom))

/* VSOA server stream get custom data */
#define vsoa_server_stream_custom(stream)   ((stream)->custom)

#ifdef __cplusplus
}
#endif

#endif /* VSOA_SERVER_H */
/*
 * end
 */
