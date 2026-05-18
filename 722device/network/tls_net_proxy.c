#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <fcntl.h>
#include "tls_net_proxy.h"
#include "tls_log.h"
#include "tls_server.h"
#include "app_vsoa_publish.h"
#include "cJSON.h"

net_proxy_t g_net_proxy = {
    .tcp_fd = -1,
    .udp_fd = -1,
    .config = {0},
    .stop_listening_ = false,
    .tls_server_ = NULL,
    .app_publish_ = NULL
};

static void net_proxy_prio_queue_init(net_proxy_prio_queue_t* queue)
{
    memset(queue, 0, sizeof(net_proxy_prio_queue_t));
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
}

static void net_proxy_prio_queue_destroy(net_proxy_prio_queue_t* queue)
{
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
}

static bool net_proxy_prio_queue_push(net_proxy_prio_queue_t* queue, net_proxy_prio_node_t* node)
{
    bool result = false;
    pthread_mutex_lock(&queue->lock);
    if (queue->count < NET_PROXY_QUEUE_SIZE)
    {
        memcpy(&queue->nodes[queue->tail], node, sizeof(net_proxy_prio_node_t));
        queue->tail = (queue->tail + 1) % NET_PROXY_QUEUE_SIZE;
        queue->count++;
        pthread_cond_signal(&queue->cond);
        result = true;
    }
    pthread_mutex_unlock(&queue->lock);
    return result;
}

static bool net_proxy_prio_queue_pop(net_proxy_prio_queue_t* queue, net_proxy_prio_node_t* node)
{
    bool result = false;
    pthread_mutex_lock(&queue->lock);
    if (queue->count > 0)
    {
        memcpy(node, &queue->nodes[queue->head], sizeof(net_proxy_prio_node_t));
        queue->head = (queue->head + 1) % NET_PROXY_QUEUE_SIZE;
        queue->count--;
        result = true;
    }
    pthread_mutex_unlock(&queue->lock);
    return result;
}

// Push transmission data to priority queue (for TCP/UDP send)
bool tls_net_proxy_push_to_prio_queue(net_proxy_t* proxy, const char* dest_ip, U16 dest_port,
                                       link_proto_t proto, U8 priority,
                                       const char* url, U32 url_len,
                                       U8* data, U32 data_len,
                                       U8* param, U32 param_len)
{
    net_proxy_prio_node_t node;
    if (NULL == proxy || NULL == dest_ip || NULL == url || NULL == data)
        return false;

    memset(&node, 0, sizeof(node));

    // Fill transmit data
    strncpy(node.transmit_data.dest_ip, dest_ip, MAX_IP_LENGTH - 1);
    node.transmit_data.dest_port = dest_port;
    node.transmit_data.proto = proto;
    node.transmit_data.priority = priority;

    if (url_len > 0 && url_len < sizeof(node.transmit_data.url))
        memcpy(node.transmit_data.url, url, url_len);
    node.transmit_data.url_len = (url_len < sizeof(node.transmit_data.url)) ? url_len : sizeof(node.transmit_data.url) - 1;

    node.transmit_data.data = data;
    node.transmit_data.data_len = data_len;
    node.transmit_data.param = param;
    node.transmit_data.param_len = param_len;

    // Clamp priority to valid range
    if (priority >= MAX_PRIORITY_LEVELS)
        priority = 0;
    node.priority = priority;

    return net_proxy_prio_queue_push(&proxy->prio_queues[priority], &node);
}

// Load config using cJSON
S32 tls_net_proxy_load_config_json(const char* config_path, link_map_config_t* config)
{
    FILE *fp;
    char buffer[4096];
    int len;
    cJSON *root;
    cJSON *item;
    cJSON *entries;
    int i, size;

    if (NULL == config_path || NULL == config)
        return -1;

    memset(config, 0, sizeof(link_map_config_t));

    fp = fopen(config_path, "rb");
    if (!fp)
    {
        LOG_ERROR("Failed to open config file: %s", config_path);
        return -1;
    }

    len = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[len] = '\0';
    fclose(fp);

    root = cJSON_Parse(buffer);
    if (!root)
    {
        LOG_ERROR("Failed to parse JSON config file: %s", config_path);
        return -1;
    }

    // Get count
    item = cJSON_GetObjectItem(root, "count");
    if (item && cJSON_IsNumber(item))
        config->count = item->valueint;

    // Get default_port
    item = cJSON_GetObjectItem(root, "default_port");
    if (item && cJSON_IsNumber(item))
        config->default_port = (U16)item->valueint;

    // Get listen_tcp_port
    item = cJSON_GetObjectItem(root, "listen_tcp_port");
    if (item && cJSON_IsNumber(item))
        config->listen_tcp_port = (U16)item->valueint;

    // Get listen_udp_port
    item = cJSON_GetObjectItem(root, "listen_udp_port");
    if (item && cJSON_IsNumber(item))
        config->listen_udp_port = (U16)item->valueint;

    // Get entries array
    entries = cJSON_GetObjectItem(root, "entries");
    if (entries && cJSON_IsArray(entries))
    {
        size = cJSON_GetArraySize(entries);
        if (size > MAX_LINK_MAP_COUNT)
            size = MAX_LINK_MAP_COUNT;

        for (i = 0; i < size; i++)
        {
            cJSON *entry = cJSON_GetArrayItem(entries, i);
            cJSON *id_val = cJSON_GetObjectItem(entry, "id");
            cJSON *ip_val = cJSON_GetObjectItem(entry, "dest_ip");
            cJSON *port_val = cJSON_GetObjectItem(entry, "dest_port");

            if (id_val && ip_val && port_val)
            {
                config->entries[i].id = id_val->valueint;
                strncpy(config->entries[i].dest_ip, ip_val->valuestring, MAX_IP_LENGTH - 1);
                config->entries[i].dest_ip[MAX_IP_LENGTH - 1] = '\0';
                config->entries[i].dest_port = (U16)port_val->valueint;
                LOG_INFO("Loaded entry: id=%d, ip=%s, port=%d",
                         config->entries[i].id, config->entries[i].dest_ip, config->entries[i].dest_port);
            }
        }
        config->count = size;
    }

    cJSON_Delete(root);
    LOG_INFO("Loaded %d link map entries", config->count);
    return 0;
}

link_map_entry_t* tls_net_proxy_find_entry(link_map_config_t* config, U32 id)
{
    U32 i;
    if (NULL == config)
        return NULL;

    for (i = 0; i < config->count; i++)
    {
        if (config->entries[i].id == id)
            return &config->entries[i];
    }
    return NULL;
}

TLS_RESULT tls_net_proxy_init(link_map_config_t* config)
{
    int i;
    if (NULL == config)
        return TLS_RESULT_E_INVALID_PARAM;

    memcpy(&g_net_proxy.config, config, sizeof(link_map_config_t));
    g_net_proxy.tcp_fd = -1;
    g_net_proxy.udp_fd = -1;
    g_net_proxy.stop_listening_ = false;
    g_net_proxy.tls_server_ = NULL;
    g_net_proxy.app_publish_ = NULL;

    // Initialize priority queues
    for (i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        net_proxy_prio_queue_init(&g_net_proxy.prio_queues[i]);
    }

    return TLS_RESULT_S_OK;
}

void tls_net_proxy_set_server(void* tls_server)
{
    g_net_proxy.tls_server_ = tls_server;
}

void tls_net_proxy_set_publish(app_vsoa_publish_t* publish)
{
    g_net_proxy.app_publish_ = publish;
}

void tls_net_proxy_uninit(void)
{
    int i;
    for (i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        net_proxy_prio_queue_destroy(&g_net_proxy.prio_queues[i]);
    }
    if (g_net_proxy.tcp_fd >= 0)
    {
        close(g_net_proxy.tcp_fd);
        g_net_proxy.tcp_fd = -1;
    }
    if (g_net_proxy.udp_fd >= 0)
    {
        close(g_net_proxy.udp_fd);
        g_net_proxy.udp_fd = -1;
    }
}

S32 tls_net_proxy_send_tcp(const char* dest_ip, U16 dest_port, const char* url, U32 url_len, const U8* data, U32 size)
{
    S32 fd;
    struct sockaddr_in addr;
    S32 ret;
    U8* send_buf;
    U32 send_buf_size;
    U32 offset = 0;
    int flags;
    int err;
    socklen_t err_len;
    struct timeval tv;
    fd_set write_fds;

    if (NULL == dest_ip || NULL == data || 0 == size)
        return -1;

    // Allocate buffer: url_len(4 bytes) + url + data
    send_buf_size = 4 + url_len + size;
    send_buf = (U8*)malloc(send_buf_size);
    if (!send_buf)
        return -1;

    // Encode: url_len(4 bytes, network order) + url + payload
    send_buf[0] = (url_len >> 24) & 0xFF;
    send_buf[1] = (url_len >> 16) & 0xFF;
    send_buf[2] = (url_len >> 8) & 0xFF;
    send_buf[3] = url_len & 0xFF;
    offset = 4;

    if (url_len > 0 && url)
    {
        memcpy(send_buf + offset, url, url_len);
        offset += url_len;
    }

    memcpy(send_buf + offset, data, size);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        LOG_ERROR("TCP socket create failed: %s", strerror(errno));
        free(send_buf);
        return -1;
    }

    // Set send timeout to 5 seconds
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    // Set connect timeout to 5 seconds using non-blocking mode
    flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &addr.sin_addr);

    ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0)
    {
        if (errno == EINPROGRESS)
        {
            FD_ZERO(&write_fds);
            FD_SET(fd, &write_fds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            ret = select(fd + 1, NULL, &write_fds, NULL, &tv);
            if (ret == 0)
            {
                LOG_ERROR("TCP connect to %s:%d timeout", dest_ip, dest_port);
                close(fd);
                free(send_buf);
                return -1;
            }
            else if (ret < 0)
            {
                LOG_ERROR("TCP connect select failed: %s", strerror(errno));
                close(fd);
                free(send_buf);
                return -1;
            }
            else
            {
                // Check if connection actually succeeded
                err = 0;
                err_len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
                if (err != 0)
                {
                    LOG_ERROR("TCP connect to %s:%d failed: %s", dest_ip, dest_port, strerror(err));
                    close(fd);
                    free(send_buf);
                    return -1;
                }
            }
        }
        else
        {
            LOG_ERROR("TCP connect to %s:%d failed: %s", dest_ip, dest_port, strerror(errno));
            close(fd);
            free(send_buf);
            return -1;
        }
    }

    // Restore blocking mode
    fcntl(fd, F_SETFL, flags);

    ret = send(fd, send_buf, send_buf_size, 0);
    if (ret > 0)
    {
        LOG_DEBUG("TCP sent %d bytes to %s:%d", ret, dest_ip, dest_port);
    }

    close(fd);
    free(send_buf);
    return ret;
}

S32 tls_net_proxy_send_udp(const char* dest_ip, U16 dest_port, const char* url, U32 url_len, const U8* data, U32 size)
{
    S32 fd;
    struct sockaddr_in addr;
    S32 ret;
    U8* send_buf;
    U32 send_buf_size;
    U32 offset = 0;

    if (NULL == dest_ip || NULL == data || 0 == size)
        return -1;

    // Allocate buffer: url_len(4 bytes) + url + data
    send_buf_size = 4 + url_len + size;
    send_buf = (U8*)malloc(send_buf_size);
    if (!send_buf)
        return -1;

    // Encode: url_len(4 bytes, network order) + url + payload
    send_buf[0] = (url_len >> 24) & 0xFF;
    send_buf[1] = (url_len >> 16) & 0xFF;
    send_buf[2] = (url_len >> 8) & 0xFF;
    send_buf[3] = url_len & 0xFF;
    offset = 4;

    if (url_len > 0 && url)
    {
        memcpy(send_buf + offset, url, url_len);
        offset += url_len;
    }

    memcpy(send_buf + offset, data, size);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        LOG_ERROR("UDP socket create failed: %s", strerror(errno));
        free(send_buf);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &addr.sin_addr);

    ret = sendto(fd, send_buf, send_buf_size, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (ret > 0)
    {
        LOG_DEBUG("UDP sent %d bytes to %s:%d", ret, dest_ip, dest_port);
    }

    close(fd);
    free(send_buf);
    return ret;
}

// Parse url and payload from buffer
// Buffer format: url_len(4 bytes) + url(url_len bytes) + payload_data(remaining)
// Returns: 0 on success, -1 on failure
static int parse_url_payload_from_buffer(const U8* buffer, U32 buffer_len,
                                         char* url_out, U32 url_out_size,
                                         U8** payload_out, U32* payload_len_out)
{
    U32 url_len;
    U32 offset = 0;

    if (NULL == buffer || buffer_len < 4)
        return -1;

    // Extract URL length (first 4 bytes, network byte order)
    url_len = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    offset = 4;

    // Validate URL length
    if (url_len >= url_out_size || offset + url_len > buffer_len)
        return -1;

    // Extract URL
    memcpy(url_out, buffer + offset, url_len);
    url_out[url_len] = '\0';
    offset += url_len;

    // Extract payload
    if (payload_out && payload_len_out)
    {
        *payload_out = (U8*)(buffer + offset);
        *payload_len_out = buffer_len - offset;
    }

    return 0;
}

// TCP listen thread - receives data and directly publishes via VSOA
static void* tls_net_proxy_tcp_listen_thread(void* arg)
{
    net_proxy_t* proxy = (net_proxy_t*)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    S32 client_fd;
    U8 buffer[4096];
    S32 ret;
    vsoa_url_t vsoa_url;
    vsoa_payload_t vsoa_payload;

    LOG_INFO("TCP listen thread started, port %d", proxy->config.listen_tcp_port);

    while (!proxy->stop_listening_)
    {
        if (proxy->tcp_fd < 0)
            break;

        fd_set read_fds;
        struct timeval tv = {1, 0};

        FD_ZERO(&read_fds);
        FD_SET(proxy->tcp_fd, &read_fds);

        ret = select(proxy->tcp_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0)
        {
            if (proxy->stop_listening_)
                break;
            continue;
        }
        else if (ret == 0)
        {
            continue;
        }

        client_fd = accept(proxy->tcp_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            if (proxy->stop_listening_)
                break;
            continue;
        }

        memset(&vsoa_url, 0, sizeof(vsoa_url));
        memset(&vsoa_payload, 0, sizeof(vsoa_payload));

        char url[256];
        U8* payload = NULL;
        U32 payload_len = 0;

        ret = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (ret > 0)
        {
            LOG_INFO("TCP received %d bytes from %s:%d",
                     ret, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Parse URL and payload from buffer
            if (parse_url_payload_from_buffer(buffer, ret, url, sizeof(url), &payload, &payload_len) == 0)
            {
                vsoa_url.url = url;
                vsoa_url.url_len = strlen(url);
                vsoa_payload.data = payload;
                vsoa_payload.data_len = payload_len;
            }
            else
            {
                // Fallback: use /data as default topic
                vsoa_url.url = "/data";
                vsoa_url.url_len = strlen(vsoa_url.url);
                vsoa_payload.data = buffer;
                vsoa_payload.data_len = ret;
            }
            vsoa_payload.param = NULL;
            vsoa_payload.param_len = 0;

            if (proxy->app_publish_)
            {
                app_vsoa_publish_data(proxy->app_publish_, &vsoa_url, &vsoa_payload);
                LOG_INFO("TCP data published via VSOA, URL: %s, size: %d", vsoa_url.url, vsoa_payload.data_len);
            }
        }

        close(client_fd);
    }

    LOG_INFO("TCP listen thread exited");
    return NULL;
}

// UDP listen thread - receives data and directly publishes via VSOA
static void* tls_net_proxy_udp_listen_thread(void* arg)
{
    net_proxy_t* proxy = (net_proxy_t*)arg;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    U8 buffer[4096];
    S32 ret;
    vsoa_url_t vsoa_url;
    vsoa_payload_t vsoa_payload;

    LOG_INFO("UDP listen thread started, port %d", proxy->config.listen_udp_port);

    while (!proxy->stop_listening_)
    {
        if (proxy->udp_fd < 0)
            break;

        fd_set read_fds;
        struct timeval tv = {1, 0};

        FD_ZERO(&read_fds);
        FD_SET(proxy->udp_fd, &read_fds);

        ret = select(proxy->udp_fd + 1, &read_fds, NULL, NULL, &tv);
        if (ret < 0)
        {
            if (proxy->stop_listening_)
                break;
            continue;
        }
        else if (ret == 0)
        {
            continue;
        }

        memset(&vsoa_url, 0, sizeof(vsoa_url));
        memset(&vsoa_payload, 0, sizeof(vsoa_payload));

        char url[256];
        U8* payload = NULL;
        U32 payload_len = 0;

        ret = recvfrom(proxy->udp_fd, buffer, sizeof(buffer) - 1, 0,
                       (struct sockaddr*)&server_addr, &addr_len);
        if (ret > 0)
        {
            LOG_INFO("UDP received %d bytes from %s:%d",
                     ret, inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

            // Parse URL and payload from buffer
            if (parse_url_payload_from_buffer(buffer, ret, url, sizeof(url), &payload, &payload_len) == 0)
            {
                vsoa_url.url = url;
                vsoa_url.url_len = strlen(url);
                vsoa_payload.data = payload;
                vsoa_payload.data_len = payload_len;
            }
            else
            {
                // Fallback: use /data as default topic
                vsoa_url.url = "/data";
                vsoa_url.url_len = strlen(vsoa_url.url);
                vsoa_payload.data = buffer;
                vsoa_payload.data_len = ret;
            }
            vsoa_payload.param = NULL;
            vsoa_payload.param_len = 0;

            if (proxy->app_publish_)
            {
                app_vsoa_publish_data(proxy->app_publish_, &vsoa_url, &vsoa_payload);
                LOG_INFO("UDP data published via VSOA, URL: %s, size: %d", vsoa_url.url, vsoa_payload.data_len);
            }
        }
    }

    LOG_INFO("UDP listen thread exited");
    return NULL;
}

// Check if there's higher priority data waiting (for preemption)
static bool check_higher_priority_waiting(net_proxy_t* proxy, U8 current_priority)
{
    int i;
    for (i = MAX_PRIORITY_LEVELS - 1; i > current_priority; i--)
    {
        pthread_mutex_lock(&proxy->prio_queues[i].lock);
        if (proxy->prio_queues[i].count > 0)
        {
            pthread_mutex_unlock(&proxy->prio_queues[i].lock);
            return true;
        }
        pthread_mutex_unlock(&proxy->prio_queues[i].lock);
    }
    return false;
}

// Transmit thread - pops from priority queues (high priority first) with preemption support
static void* tls_net_proxy_transmit_thread(void* arg)
{
    net_proxy_t* proxy = (net_proxy_t*)arg;
    net_proxy_prio_node_t node;
    int i;
    S32 sent;
    bool popped;

    LOG_INFO("Transmit thread started with %d priority levels (0-%d)", MAX_PRIORITY_LEVELS, MAX_PRIORITY_LEVELS - 1);

    while (!proxy->stop_listening_)
    {
        popped = false;

        // Check priority queues from high to low
        for (i = MAX_PRIORITY_LEVELS - 1; i >= 0; i--)
        {
            pthread_mutex_lock(&proxy->prio_queues[i].lock);
            if (proxy->prio_queues[i].count > 0)
            {
                memcpy(&node, &proxy->prio_queues[i].nodes[proxy->prio_queues[i].head], sizeof(net_proxy_prio_node_t));
                proxy->prio_queues[i].head = (proxy->prio_queues[i].head + 1) % NET_PROXY_QUEUE_SIZE;
                proxy->prio_queues[i].count--;
                popped = true;
            }
            pthread_mutex_unlock(&proxy->prio_queues[i].lock);

            if (popped)
                break;
        }

        if (!popped)
        {
            // All queues empty, sleep and retry
            usleep(1000);  // 1ms
            continue;
        }

        // Preemption check: before sending, check if higher priority data arrived
        while (!proxy->stop_listening_)
        {
            if (check_higher_priority_waiting(proxy, node.priority))
            {
                // Higher priority data waiting, put current back to its queue
                pthread_mutex_lock(&proxy->prio_queues[node.priority].lock);
                if (proxy->prio_queues[node.priority].count < NET_PROXY_QUEUE_SIZE)
                {
                    memcpy(&proxy->prio_queues[node.priority].nodes[proxy->prio_queues[node.priority].tail],
                           &node, sizeof(net_proxy_prio_node_t));
                    proxy->prio_queues[node.priority].tail = (proxy->prio_queues[node.priority].tail + 1) % NET_PROXY_QUEUE_SIZE;
                    proxy->prio_queues[node.priority].count++;
                    LOG_INFO("Preemption: higher priority data arrived, requeuing priority %d data", node.priority);
                }
                pthread_mutex_unlock(&proxy->prio_queues[node.priority].lock);

                // Break to re-check all queues from highest priority
                break;
            }

            // Send data via TCP or UDP
            sent = -1;
            if (node.transmit_data.proto == PROTO_TCP)
            {
                sent = tls_net_proxy_send_tcp(node.transmit_data.dest_ip, node.transmit_data.dest_port,
                                               node.transmit_data.url, node.transmit_data.url_len,
                                               node.transmit_data.data, node.transmit_data.data_len);
            }
            else if (node.transmit_data.proto == PROTO_UDP)
            {
                sent = tls_net_proxy_send_udp(node.transmit_data.dest_ip, node.transmit_data.dest_port,
                                              node.transmit_data.url, node.transmit_data.url_len,
                                              node.transmit_data.data, node.transmit_data.data_len);
            }

            if (sent > 0)
            {
                LOG_INFO("Transmitted %d bytes to %s:%d via %s, priority %d, topic %s",
                         sent, node.transmit_data.dest_ip, node.transmit_data.dest_port,
                         node.transmit_data.proto == PROTO_TCP ? "TCP" : "UDP",
                         node.priority, node.transmit_data.url);
            }
            else
            {
                LOG_ERROR("Failed to transmit to %s:%d via %s",
                          node.transmit_data.dest_ip, node.transmit_data.dest_port,
                          node.transmit_data.proto == PROTO_TCP ? "TCP" : "UDP");
            }

            // After send completes, loop back to check for higher priority data
            // This implements the "preemption after completion" behavior
            break;
        }
    }

    LOG_INFO("Transmit thread exited");
    return NULL;
}

S32 tls_net_proxy_start_listening(net_proxy_t* proxy)
{
    struct sockaddr_in addr;
    S32 opt = 1;

    if (NULL == proxy)
        return -1;

    if (proxy->config.listen_tcp_port > 0)
    {
        proxy->tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (proxy->tcp_fd < 0)
        {
            LOG_ERROR("TCP listen socket create failed");
            return -1;
        }

        setsockopt(proxy->tcp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(proxy->config.listen_tcp_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(proxy->tcp_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            LOG_ERROR("TCP bind failed on port %d", proxy->config.listen_tcp_port);
            close(proxy->tcp_fd);
            proxy->tcp_fd = -1;
            return -1;
        }

        if (listen(proxy->tcp_fd, 10) < 0)
        {
            LOG_ERROR("TCP listen failed");
            close(proxy->tcp_fd);
            proxy->tcp_fd = -1;
            return -1;
        }

        LOG_INFO("TCP listening on port %d", proxy->config.listen_tcp_port);

        proxy->stop_listening_ = false;
        if (pthread_create(&proxy->tcp_listen_thread_, NULL, tls_net_proxy_tcp_listen_thread, proxy) != 0)
        {
            LOG_ERROR("Failed to create TCP listen thread");
            close(proxy->tcp_fd);
            proxy->tcp_fd = -1;
            return -1;
        }
    }

    if (proxy->config.listen_udp_port > 0)
    {
        proxy->udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (proxy->udp_fd < 0)
        {
            LOG_ERROR("UDP listen socket create failed");
            return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(proxy->config.listen_udp_port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(proxy->udp_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            LOG_ERROR("UDP bind failed on port %d", proxy->config.listen_udp_port);
            close(proxy->udp_fd);
            proxy->udp_fd = -1;
            return -1;
        }

        LOG_INFO("UDP listening on port %d", proxy->config.listen_udp_port);

        proxy->stop_listening_ = false;
        if (pthread_create(&proxy->udp_listen_thread_, NULL, tls_net_proxy_udp_listen_thread, proxy) != 0)
        {
            LOG_ERROR("Failed to create UDP listen thread");
            close(proxy->udp_fd);
            proxy->udp_fd = -1;
            return -1;
        }
    }

    // Start transmit thread (for priority queue TCP/UDP send)
    proxy->stop_listening_ = false;
    if (pthread_create(&proxy->transmit_thread_, NULL, tls_net_proxy_transmit_thread, proxy) != 0)
    {
        LOG_ERROR("Failed to create transmit thread");
        return -1;
    }

    return 0;
}

void tls_net_proxy_stop_listening(net_proxy_t* proxy)
{
    int i;
    if (NULL == proxy)
        return;

    proxy->stop_listening_ = true;

    if (proxy->tcp_fd >= 0)
    {
        close(proxy->tcp_fd);
        proxy->tcp_fd = -1;
    }
    if (proxy->udp_fd >= 0)
    {
        close(proxy->udp_fd);
        proxy->udp_fd = -1;
    }

    if (proxy->tcp_listen_thread_ != 0)
    {
        pthread_join(proxy->tcp_listen_thread_, NULL);
        proxy->tcp_listen_thread_ = 0;
    }
    if (proxy->udp_listen_thread_ != 0)
    {
        pthread_join(proxy->udp_listen_thread_, NULL);
        proxy->udp_listen_thread_ = 0;
    }
    if (proxy->transmit_thread_ != 0)
    {
        pthread_join(proxy->transmit_thread_, NULL);
        proxy->transmit_thread_ = 0;
    }

    for (i = 0; i < MAX_PRIORITY_LEVELS; i++)
    {
        net_proxy_prio_queue_destroy(&proxy->prio_queues[i]);
    }
}