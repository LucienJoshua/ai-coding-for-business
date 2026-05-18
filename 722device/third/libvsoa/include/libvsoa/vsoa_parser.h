/*
 * Copyright (c) 2021 ACOAUTO Team.
 * All rights reserved.
 *
 * Detailed license information can be found in the LICENSE file.
 *
 * File: vsoa_parser.h Vehicle SOA parser.
 *
 * Author: Han.hui <hanhui@acoinfo.com>
 *
 */

#ifndef VSOA_PARSER_H
#define VSOA_PARSER_H

/*
 * This VSOA parsing library does not use dynamic memory to ensure safety and real-time performance.
 */

/* Whether to print an error when an error packet is received */
#define VSOA_DEBUG_PRINT_PARSER_ERROR  0

/*
 * Vehicle SOA Protocol Header (Over TCP / TLS):
 *
 *  0               1               2               3
 *  0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | MAGIC |  VER  |     TYPE      |R|T|S|     |Pad|  Status Code  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |              Sequence Number / Client ID (Quick)              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Tunnel ID            |   Resource ID Length (URL)    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Parameter Length                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           Data Length                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * MAGIC : Must be: 0x9
 * VER   : Must be: 0x2
 *
 * TYPE                     DIRECTION   REPLY   DATA                    DESC
 * 0x00 ServInfo            C <---> S   Y       Para Data               Get server information.
 * 0x01 RemoteCall          C <---> S   Y       Para/Bin Data           RPC call / reply.
 * 0x02 Subscribe           C <---> S   Y       No Data                 Subscribe event.
 * 0x03 Unsubscribe         C <---> S   Y       No Data                 Unsubscribe event.
 * 0x04 Publish             C <---  S   N       Para/Bin Data           Publish event.
 * 0x05 Datagram            C <---> S   N       Para/Bin Data           Datagram packets.
 * 0x06 QoSSetup            C <---  S   N       QoS Setup               Setup Client QoS.
 * 0xfe Noop                C <---> S   N       Noop                    Noop packets.
 * 0xff Ping/Echo           C <---> S   Y       No Data                 Ping / Echo packets.
 *
 * Flags
 * R: Reply, confirm or ping echo.
 * T: Has tunnel id.
 * S: Set / Get method.
 *
 * Pad: It must be ensured that a packet of VSOA is 32-bit aligned length,
 *      so Pad indicates the number of invalid characters added in this packet for 32-bit alignment.
 *
 * Status Code: Specify the return status when the VSOA server returns,
 *              0 means success, others mean errors.
 *              In QoSSetup packet, this feild means client priority.
 *
 * SeqNo: RPC call, Subscribe and Unsubscribe and other data that need to be replied use sequence number.
 *        The server response packet will use the same sequence number.
 *
 * Client ID: Only used when the client sends quick data to the server.
 *
 * Tunnel ID: If `flags` has a `T` flag, this packet indicates a tunnel link information,
 *            usually a port number.
 *
 * Resource ID Length: The resource ID length specified by this packet, usually in standard URL format.
 *
 * Parameter Length: The parameter length of this data packet,
 *                   the parameter content is usually JSON data type.
 *
 * Data Length: The binary payload length of this packet can be in any format.
 *
 * Max variable length: URL length + Parameter Length + Data Length must less than or equal to 256KB - 20
 * 
 */

/* VSOA header length */
#define VSOA_HDR_LENGTH  20

/* VSOA max packet length (32 bits align) */
#define VSOA_MAX_PACKET_LENGTH  262144

/* VSOA max payload length */
#define VSOA_MAX_DATA_LENGTH  (VSOA_MAX_PACKET_LENGTH - VSOA_HDR_LENGTH)

/* VSOA max quick packet length (MAX UDP packet length) */
#define VSOA_MAX_QPACKET_LENGTH  65507

/* VSOA max payload length on quick channel */
#define VSOA_MAX_QDATA_LENGTH  (VSOA_MAX_QPACKET_LENGTH - VSOA_HDR_LENGTH)

/* VSOA magic and version */
#define VSOA_MAGIC    0x9
#define VSOA_VERSION  0x2

/* VSOA header types */
#define VSOA_TYPE_SERVINFO     0x00
#define VSOA_TYPE_RPC          0x01
#define VSOA_TYPE_SUBSCRIBE    0x02
#define VSOA_TYPE_UNSUBSCRIBE  0x03
#define VSOA_TYPE_PUBLISH      0x04
#define VSOA_TYPE_DATAGRAM     0x05
#define VSOA_TYPE_QOSSETUP     0x06
#define VSOA_TYPE_NOOP         0xfe
#define VSOA_TYPE_PINGECHO     0xff

/* VSOA header flags */
#define VSOA_FLAG_REPLY   0x1
#define VSOA_FLAG_TUNNEL  0x2
#define VSOA_FLAG_SET     0x4

/* VSOA header pad */
#define VSOA_PAD_MASK  0xc0
#define VSOA_PAD_SHIFT 6

/* VSOA status code */
#define VSOA_STATUS_SUCCESS        0
#define VSOA_STATUS_PASSWORD       1
#define VSOA_STATUS_ARGUMENTS      2
#define VSOA_STATUS_INVALID_URL    3
#define VSOA_STATUS_NO_RESPONDING  4
#define VSOA_STATUS_NO_PERMISSIONS 5
#define VSOA_STATUS_NO_MEMORY      6

/* Headers */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VSOA header (multibyte members use network byte order) */
typedef struct {
    uint8_t magic_ver;
    uint8_t type;
    uint8_t flags;
    uint8_t status;
    uint32_t seqno;
    uint16_t tunid;
    uint16_t url_len;
    uint32_t param_len;
    uint32_t data_len;
} vsoa_header_t;

/* VSOA receiver state and buffer */
typedef struct {
    uint32_t cur_len;
    uint32_t total_len;
    uint8_t buffer[VSOA_MAX_PACKET_LENGTH];
} vsoa_recv_t;

/* VSOA url */
typedef struct {
    char *url;
    size_t url_len;
} vsoa_url_t;

/* VSOA url initializer and setter */
#define VSOA_URL_INITIALIZER \
        { NULL, 0 }
#define VSOA_URL_SET(u, p, p_len) \
        { (u)->url = (p); (u)->url_len = (p_len); }

/* VSOA payload */
typedef struct {
    char *param;
    size_t param_len;
    void *data;
    size_t data_len;
} vsoa_payload_t;

/* VSOA payload initializer and setter */
#define VSOA_PAYLOAD_INITIALIZER \
        { NULL, 0, NULL, 0 }
#define VSOA_PAYLOAD_SET(pl, p, p_len, d, d_len) \
        { (pl)->param = (p); (pl)->param_len = (p_len); \
          (pl)->data  = (d); (pl)->data_len  = (d_len); }

/* VSOA packet input callback */
typedef bool (*vsoa_input_callback_t)(void *arg, vsoa_header_t *vsoa_hdr);

/* Initialize VSOA header (`outb` must have at least VSOA_MAX_PACKET_LENGTH bytes) */
vsoa_header_t *vsoa_parser_init_header(void *outb, uint8_t type, uint8_t flags, uint8_t status, uint32_t seqno);

/* Initialize VSOA receiver */
void vsoa_parser_init_recv(vsoa_recv_t *recv);

/* Set correctly `Pad` before data packet transmission return packet size include pad */
size_t vsoa_parser_fixp_length(vsoa_header_t *vsoa_hdr);

/* Calculate the package need memory size
 * Used to allocate dynamic send buffers */
size_t vsoa_parser_calc_size(const vsoa_url_t *url, const vsoa_payload_t *payload, uint8_t *pad);

/* Set VSOA header pad size manually
 * `pad` value must be obtained from vsoa_parser_calc_size()! */
bool vsoa_parser_set_pad(vsoa_header_t *vsoa_hdr, uint8_t pad);

/* Set VSOA header tunnel port (`tunid` is network byte order) */
bool vsoa_parser_set_tunid(vsoa_header_t *vsoa_hdr, uint16_t tunid);

/* Set VSOA url */
bool vsoa_parser_set_url(vsoa_header_t *vsoa_hdr, const vsoa_url_t *url);

/* Set VSOA payload */
bool vsoa_parser_set_payload(vsoa_header_t *vsoa_hdr, const vsoa_payload_t *payload);

/* Get VSOA packet length (not include `Pad` size) */
size_t vsoa_parser_get_length(vsoa_header_t *vsoa_hdr);

/* Get VSOA url */
bool vsoa_parser_get_url(const vsoa_header_t *vsoa_hdr, vsoa_url_t *url);

/* Get VSOA payload */
bool vsoa_parser_get_payload(const vsoa_header_t *vsoa_hdr, vsoa_payload_t *payload);

/* VSOA input */
bool vsoa_parser_input(vsoa_recv_t *recv, void *buf, size_t buf_len,
                       vsoa_input_callback_t callback, void *arg);

/* VSOA packet input */
vsoa_header_t *vsoa_parser_packet_input(void *buf, size_t buf_len);

/* VSOA header set seqno */
#define vsoa_parser_set_seqno(vsoa_hdr, seqno) ((vsoa_hdr)->seqno = htonl(seqno))

/* VSOA header get seqno */
#define vsoa_parser_get_seqno(vsoa_hdr) ntohl((vsoa_hdr)->seqno)

/* VSOA header get type */
#define vsoa_parser_get_type(vsoa_hdr)  ((vsoa_hdr)->type)

/* VSOA header get flags */
#define vsoa_parser_get_flags(vsoa_hdr)  ((vsoa_hdr)->flags)

/* VSOA header get status */
#define vsoa_parser_get_status(vsoa_hdr)  ((vsoa_hdr)->status)

/* VSOA header get tunid (`tunid` is network byte order) */
#define vsoa_parser_get_tunid(vsoa_hdr)  ((vsoa_hdr)->tunid)

/* VSOA header get url length */
#define vsoa_parser_get_url_len(vsoa_hdr)  ntohs((vsoa_hdr)->url_len)

/* VSOA header get url length */
#define vsoa_parser_get_param_len(vsoa_hdr)  ntohl((vsoa_hdr)->param_len)

/* VSOA header get url length */
#define vsoa_parser_get_data_len(vsoa_hdr)  ntohl((vsoa_hdr)->data_len)

/* VSOA library version */
const char *vsoa_parser_version(void);

/* VSOA print header */
void vsoa_parser_print_header(const vsoa_header_t *vsoa_hdr, bool data_detail);

#ifdef __cplusplus
}
#endif

#endif /* VSOA_PARSER_H */
/*
 * end
 */
