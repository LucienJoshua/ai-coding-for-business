/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_position.h Vehicle SOA position.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_POSITION_H
#define VSOA_POSITION_H

#include <time.h>
#include <stdbool.h>
#include <sys/socket.h>

/* Max server name length */
#define VSOA_POSITION_MAX_NAME_LEN  512

#ifdef __cplusplus
extern "C" {
#endif

/* Structure declaration */
struct vsoa_position_server;
struct vsoa_position_response;

/* Position server type */
typedef struct vsoa_position_server vsoa_position_server_t;

/* Position server response type */
typedef struct vsoa_position_response vsoa_position_response_t;

/* VSOA position server query callback */
typedef void (*vsoa_position_query_func_t)(void *arg, int domain, const char *query_name,
                                           vsoa_position_response_t *response);

/* Create VSOA position server */
vsoa_position_server_t *vsoa_position_server_create(vsoa_position_query_func_t query, void *arg);

/* VSOA position server close */
void vsoa_position_server_close(vsoa_position_server_t *server);

/* VSOA position server start */
bool vsoa_position_server_start(vsoa_position_server_t *server, const struct sockaddr *pos_addr, socklen_t addr_len);

/* VSOA position server checking fd */
int vsoa_position_server_fd(vsoa_position_server_t *server);

/* VSOA position server input */
void vsoa_position_server_input(vsoa_position_server_t *server);

/* VSOA position server response (server == NULL : query failed) */
void vsoa_position_server_response(vsoa_position_server_t *server, vsoa_position_response_t *response,
                                   const struct sockaddr *vsoa_serv, socklen_t addr_len, bool sec);

/* VSOA position server set custom data */
void vsoa_position_server_set_custom(vsoa_position_server_t *server, void *custom);

/* VSOA position server custom data */
void *vsoa_position_server_custom(vsoa_position_server_t *server);

/* VSOA set position lookup */
bool vsoa_position_lookup_server(const struct sockaddr *pos_addr, socklen_t addr_len);

/* VSOA position lookup (Synchronous) domain == -1 means any socket family
 * If position server is specified, only the specified server will be queried,
 * If the position server address has not been specified, this function will first use the
 * environment variable `VSOA_POS_SERVER` to query,
 * if not found, it will use the server query configured in `/etc/vsoa.pos`. */
bool vsoa_position_lookup(int domain, const char *serv_name,
                          struct sockaddr *serv_addr, socklen_t *addr_len, bool *sec, const struct timespec *timeout);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_POSITION_H */
/*
 * end
 */
