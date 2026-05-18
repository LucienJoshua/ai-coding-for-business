#ifndef __TLS_NET_PROXY_H__
#define __TLS_NET_PROXY_H__

#include "tls_type.h"
#include "tls_com.h"
#include <pthread.h>
#include <stdbool.h>

#define MAX_LINK_MAP_COUNT    64
#define MAX_IP_LENGTH         16
#define NET_PROXY_QUEUE_SIZE  256
#define MAX_PRIORITY_LEVELS   8   // Priority levels 0-7 (7 is highest)

typedef enum {
    PROTO_UDP = 1,
    PROTO_TCP = 2
} link_proto_t;

typedef struct {
    U32                 id;
    char                dest_ip[MAX_IP_LENGTH];
    U16                 dest_port;
} link_map_entry_t;

typedef struct {
    link_map_entry_t    entries[MAX_LINK_MAP_COUNT];
    U32                 count;
    U16                 default_port;
    U16                 listen_tcp_port;
    U16                 listen_udp_port;
} link_map_config_t;

// Forward declaration
typedef struct _app_vsoa_publish app_vsoa_publish_t;

// Transmission data structure - holds data to be sent via TCP/UDP
typedef struct _net_proxy_transmit_data {
    char                dest_ip[MAX_IP_LENGTH];    // Destination IP
    U16                 dest_port;                // Destination port
    link_proto_t        proto;                    // PROTO_UDP or PROTO_TCP
    U8                  priority;                 // Priority level (0-255)
    char                url[256];                 // VSOA publish URL topic path (/<topic>/...)
    U32                 url_len;
    U8*                 data;                     // Pointer to payload data
    U32                 data_len;                  // Payload data length
    U8*                 param;                     // Pointer to param data
    U32                 param_len;                 // Param data length
} net_proxy_transmit_data_t;

// Priority queue node - contains pointers to url and payload data
typedef struct _net_proxy_prio_node {
    net_proxy_transmit_data_t  transmit_data;     // Transmission data
    U8                          priority;         // Priority level
} net_proxy_prio_node_t;

// Priority queue structure - one queue per priority level
typedef struct {
    net_proxy_prio_node_t  nodes[NET_PROXY_QUEUE_SIZE];
    U32                     head;
    U32                     tail;
    U32                     count;
    pthread_mutex_t         lock;
    pthread_cond_t          cond;
} net_proxy_prio_queue_t;

typedef struct {
    S32                 tcp_fd;
    S32                 udp_fd;
    link_map_config_t   config;
    pthread_t           tcp_listen_thread_;
    pthread_t           udp_listen_thread_;
    pthread_t           transmit_thread_;         // Priority queue transmit thread
    bool                stop_listening_;
    void*               tls_server_;             // For TLS server operations
    app_vsoa_publish_t* app_publish_;            // For direct VSOA publish
    net_proxy_prio_queue_t  prio_queues[MAX_PRIORITY_LEVELS];  // Priority queues
} net_proxy_t;

extern net_proxy_t g_net_proxy;

extern TLS_RESULT     tls_net_proxy_init(link_map_config_t* config);
extern void          tls_net_proxy_uninit(void);
extern S32           tls_net_proxy_send_tcp(const char* dest_ip, U16 dest_port, const char* url, U32 url_len, const U8* data, U32 size);
extern S32           tls_net_proxy_send_udp(const char* dest_ip, U16 dest_port, const char* url, U32 url_len, const U8* data, U32 size);
extern S32           tls_net_proxy_load_config_json(const char* config_path, link_map_config_t* config);
extern link_map_entry_t* tls_net_proxy_find_entry(link_map_config_t* config, U32 id);
extern S32           tls_net_proxy_start_listening(net_proxy_t* proxy);
extern void          tls_net_proxy_stop_listening(net_proxy_t* proxy);
extern void          tls_net_proxy_set_server(void* tls_server);
extern void          tls_net_proxy_set_publish(app_vsoa_publish_t* publish);

// Push transmission data to priority queue (for TCP/UDP send)
extern bool          tls_net_proxy_push_to_prio_queue(net_proxy_t* proxy, const char* dest_ip, U16 dest_port,
                                                      link_proto_t proto, U8 priority,
                                                      const char* url, U32 url_len,
                                                      U8* data, U32 data_len,
                                                      U8* param, U32 param_len);

#endif