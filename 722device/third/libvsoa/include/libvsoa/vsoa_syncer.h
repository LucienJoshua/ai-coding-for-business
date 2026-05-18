/*
 * Copyright (c) 2022 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_syncer.c Vehicle SOA server param synchronizer.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_SYNCER_H
#define VSOA_SYNCER_H

#include "vsoa_parser.h"
#include "vsoa_server.h"

/* Synchronizer return value */
typedef enum {
    VSOA_SYNCER_PUB_OK = 0,
    VSOA_SYNCER_SET_OK = 1,
    VSOA_SYNCER_GET_OK = 2,
    VSOA_SYNCER_ERROR  = -1
} vsoa_syncer_ret;

/* VSOA server param synchronizer publish */
#define VSOA_PARAM_SYNCER_PUBLISH(server, url, module, struc, bin, bin_len) \
({ \
    vsoa_syncer_ret ret = VSOA_SYNCER_ERROR; \
    char *json; \
    vsoa_payload_t payload_pub; \
    if (vsoa_server_is_subscribed(server, url)) { \
        json = module##_json_stringify(struc); \
        payload_pub.data = bin; \
        payload_pub.data_len = bin_len; \
        payload_pub.param = json; \
        payload_pub.param_len = strlen(json); \
        vsoa_server_publish(server, url, &payload_pub); \
        module##_json_stringify_free(json); \
        ret = VSOA_SYNCER_PUB_OK; \
    } \
    ret; \
})

/* VSOA server param synchronizer
 * NOTICE: After the checker is called, the pointer in struc is not allowed to be used again */
#define VSOA_PARAM_SYNCER_RPC(server, cli_id, module, struc, vsoa_hdr, payload, checker, arg) \
({ \
    uint8_t (*func_checker)() = (uint8_t (*)())(checker); \
    uint32_t seqno = vsoa_parser_get_seqno(vsoa_hdr); \
    bool is_set = vsoa_parser_get_flags(vsoa_hdr) & VSOA_FLAG_SET; \
    vsoa_syncer_ret ret = VSOA_SYNCER_ERROR; \
    char *json; \
    vsoa_url_t url; \
    vsoa_payload_t payload_reply; \
    if (is_set) { \
        if (!(payload)->param || !(payload)->param_len || \
            !module##_json_parse(struc, (payload)->param, (payload)->param_len)) { \
            vsoa_server_cli_reply(server, cli_id, VSOA_STATUS_ARGUMENTS, seqno, 0, NULL); \
            goto    __out; \
        } \
        if (func_checker) { \
            uint8_t status = func_checker(arg, struc, payload); \
            if (status) { \
                vsoa_server_cli_reply(server, cli_id, status, seqno, 0, NULL); \
                module##_json_parse_free(struc); \
                goto    __out; \
            } \
        } \
        vsoa_server_cli_reply(server, cli_id, VSOA_STATUS_SUCCESS, seqno, 0, NULL); \
        module##_json_parse_free(struc); \
    } \
    json = module##_json_stringify(struc); \
    if (json) { \
        payload_reply.data = NULL; \
        payload_reply.data_len = 0; \
        payload_reply.param = json; \
        payload_reply.param_len = strlen(json); \
        if (is_set) { \
            vsoa_parser_get_url(vsoa_hdr, &url); \
            vsoa_server_publish(server, &url, &payload_reply); \
            ret = VSOA_SYNCER_SET_OK; \
        } else { \
            vsoa_server_cli_reply(server, cli_id, 0, seqno, 0, &payload_reply); \
            ret = VSOA_SYNCER_GET_OK; \
        } \
        module##_json_stringify_free(json); \
    } else if (!is_set) { \
        vsoa_server_cli_reply(server, cli_id, VSOA_STATUS_NO_MEMORY, seqno, 0, NULL); \
    } \
__out: \
    ret; \
})

#endif /* VSOA_SYNCER_H */
/*
 * end
 */
