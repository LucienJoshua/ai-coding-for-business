/**
 * ============================================================
 * Common - 通用工具函数和消息队列实现
 * ============================================================
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#include "common.h"

/* ============================================================
 * 工具函数实现
 * ============================================================ */

/**
 * 判断IP是否属于10.10.111网段
 */
int is_target_network(const char *ip) {
    if (ip == NULL) {
        return 0;
    }
    return strncmp(ip, TARGET_NET, TARGET_NET_PREFIX_LEN) == 0;
}

/**
 * 设置socket为非阻塞模式
 */
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * 获取当前时间戳(毫秒)
 */
uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ============================================================
 * 消息创建和释放
 * ============================================================ */

/**
 * 创建消息结构
 */
message_t *create_message(uint8_t type, const char *payload, int payload_len, const char *src_ip) {
    if (payload == NULL || payload_len <= 0) {
        LOG_ERR("Invalid payload");
        return NULL;
    }

    message_t *msg = (message_t *)malloc(sizeof(message_t));
    if (msg == NULL) {
        LOG_ERR("malloc failed for message");
        return NULL;
    }

    memset(msg, 0, sizeof(message_t));

    /* 填充协议头 */
    msg->header.magic = PROTOCOL_MAGIC;
    msg->header.version = PROTOCOL_VERSION;
    msg->header.msg_type = type;
    msg->header.reserved = 0;

    /* 填充消息头 */
    static uint32_t g_seq_num = 0;
    msg->msg_header.seq_num = ++g_seq_num;
    msg->msg_header.timestamp = get_timestamp_ms();
    if (src_ip) {
        strncpy(msg->msg_header.src_ip, src_ip, 15);
        msg->msg_header.src_ip[15] = '\0';
    }

    /* 分配并复制负载数据 */
    msg->payload = (char *)malloc(payload_len);
    if (msg->payload == NULL) {
        LOG_ERR("malloc failed for payload");
        free(msg);
        return NULL;
    }

    memcpy(msg->payload, payload, payload_len);
    msg->payload_len = payload_len;
    msg->total_len = sizeof(protocol_header_t) + sizeof(message_header_t) + payload_len;

    return msg;
}

/**
 * 释放消息结构
 */
void free_message(message_t *msg) {
    if (msg == NULL) {
        return;
    }

    if (msg->payload != NULL) {
        free(msg->payload);
        msg->payload = NULL;
    }

    free(msg);
}

/**
 * 序列化消息到缓冲区
 */
int serialize_message(const message_t *msg, char *buffer, int buffer_size) {
    if (msg == NULL || buffer == NULL || buffer_size <= 0) {
        return -1;
    }

    int required_size = msg->total_len;
    if (buffer_size < required_size) {
        LOG_ERR("Buffer too small: %d < %d", buffer_size, required_size);
        return -1;
    }

    char *ptr = buffer;

    /* 复制协议头 */
    memcpy(ptr, &msg->header, sizeof(protocol_header_t));
    ptr += sizeof(protocol_header_t);

    /* 复制消息头 */
    memcpy(ptr, &msg->msg_header, sizeof(message_header_t));
    ptr += sizeof(message_header_t);

    /* 复制负载数据 */
    if (msg->payload != NULL && msg->payload_len > 0) {
        memcpy(ptr, msg->payload, msg->payload_len);
    }

    return required_size;
}

/**
 * 从缓冲区解析消息
 */
message_t *deserialize_message(const char *buffer, int len) {
    if (buffer == NULL || len < (int)(sizeof(protocol_header_t) + sizeof(message_header_t))) {
        LOG_ERR("Invalid buffer or length");
        return NULL;
    }

    const char *ptr = buffer;

    /* 解析协议头 */
    protocol_header_t header;
    memcpy(&header, ptr, sizeof(protocol_header_t));
    ptr += sizeof(protocol_header_t);

    /* 验证魔数 */
    if (header.magic != PROTOCOL_MAGIC) {
        LOG_ERR("Invalid protocol magic: 0x%x", header.magic);
        return NULL;
    }

    /* 解析消息头 */
    message_header_t msg_header;
    memcpy(&msg_header, ptr, sizeof(message_header_t));
    ptr += sizeof(message_header_t);

    /* 计算负载长度 */
    int payload_len = len - sizeof(protocol_header_t) - sizeof(message_header_t);
    if (payload_len < 0) {
        LOG_ERR("Invalid payload length: %d", payload_len);
        return NULL;
    }

    /* 创建消息结构 */
    message_t *msg = (message_t *)malloc(sizeof(message_t));
    if (msg == NULL) {
        LOG_ERR("malloc failed for message");
        return NULL;
    }

    memset(msg, 0, sizeof(message_t));
    memcpy(&msg->header, &header, sizeof(protocol_header_t));
    memcpy(&msg->msg_header, &msg_header, sizeof(message_header_t));

    /* 复制负载数据 */
    if (payload_len > 0) {
        msg->payload = (char *)malloc(payload_len);
        if (msg->payload == NULL) {
            LOG_ERR("malloc failed for payload");
            free(msg);
            return NULL;
        }
        memcpy(msg->payload, ptr, payload_len);
    }

    msg->payload_len = payload_len;
    msg->total_len = len;

    return msg;
}

/* ============================================================
 * 消息队列实现
 * ============================================================ */

/**
 * 初始化消息队列
 */
msg_queue_t *msg_queue_init(int capacity) {
    msg_queue_t *queue = (msg_queue_t *)malloc(sizeof(msg_queue_t));
    if (queue == NULL) {
        LOG_ERR("malloc failed for msg_queue");
        return NULL;
    }

    memset(queue, 0, sizeof(msg_queue_t));
    queue->capacity = capacity > 0 ? capacity : MSG_QUEUE_SIZE;
    queue->front = NULL;
    queue->rear = NULL;
    queue->count = 0;
    queue->shutdown = false;

    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    LOG("Message queue initialized with capacity %d", queue->capacity);
    return queue;
}

/**
 * 销毁消息队列
 */
void msg_queue_destroy(msg_queue_t *queue) {
    if (queue == NULL) {
        return;
    }

    pthread_mutex_lock(&queue->lock);
    queue->shutdown = true;

    /* 释放所有节点 */
    while (queue->front != NULL) {
        msg_queue_node_t *node = queue->front;
        queue->front = node->next;
        if (node->msg.payload != NULL) {
            free(node->msg.payload);
        }
        free(node);
    }

    pthread_mutex_unlock(&queue->lock);

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);

    free(queue);
    LOG("Message queue destroyed");
}

/**
 * 入队消息(阻塞直到成功或队列关闭)
 */
int msg_queue_push(msg_queue_t *queue, message_t *msg) {
    if (queue == NULL || msg == NULL) {
        return -1;
    }

    pthread_mutex_lock(&queue->lock);

    /* 如果队列已关闭，直接返回 */
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    /* 等待队列不满 */
    while (queue->count >= queue->capacity && !queue->shutdown) {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    /* 检查是否因关闭而退出 */
    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    /* 创建新节点 */
    msg_queue_node_t *node = (msg_queue_node_t *)malloc(sizeof(msg_queue_node_t));
    if (node == NULL) {
        LOG_ERR("malloc failed for queue node");
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }

    memset(node, 0, sizeof(msg_queue_node_t));
    memcpy(&node->msg, msg, sizeof(message_t));
    node->next = NULL;

    /* 添加到队列尾部 */
    if (queue->rear == NULL) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }

    queue->count++;

    /* 通知等待出队的线程 */
    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->lock);

    LOG("Message queued: seq=%u, type=%d, size=%d", msg->msg_header.seq_num, msg->header.msg_type, msg->payload_len);

    return 0;
}

/**
 * 出队消息(阻塞直到有消息或队列关闭或超时)
 */
message_t *msg_queue_pop(msg_queue_t *queue, int timeout_ms) {
    if (queue == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&queue->lock);

    /* 如果队列为空，等待或超时 */
    if (queue->count == 0) {
        if (timeout_ms < 0) {
            /* 无限等待 */
            while (queue->count == 0 && !queue->shutdown) {
                pthread_cond_wait(&queue->not_empty, &queue->lock);
            }
        } else {
            /* 超时等待 */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout_ms / 1000;
            ts.tv_nsec += (timeout_ms % 1000) * 1000000;

            int ret = pthread_cond_timedwait(&queue->not_empty, &queue->lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->lock);
                return NULL;
            }
        }
    }

    /* 检查是否因关闭而退出 */
    if (queue->shutdown && queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    /* 从队列头部取出节点 */
    msg_queue_node_t *node = queue->front;
    if (node == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    queue->front = node->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }

    queue->count--;

    /* 通知等待入队的线程 */
    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->lock);

    /* 创建返回的消息副本 */
    message_t *msg = (message_t *)malloc(sizeof(message_t));
    if (msg == NULL) {
        LOG_ERR("malloc failed for return message");
        free(node);
        return NULL;
    }

    memcpy(msg, &node->msg, sizeof(message_t));
    msg->payload = node->msg.payload;  /* 转移指针所有权 */
    free(node);

    LOG("Message dequeued: seq=%u, type=%d, size=%d", msg->msg_header.seq_num, msg->header.msg_type, msg->payload_len);

    return msg;
}

/**
 * 获取队列当前消息数
 */
int msg_queue_count(msg_queue_t *queue) {
    if (queue == NULL) {
        return 0;
    }

    pthread_mutex_lock(&queue->lock);
    int count = queue->count;
    pthread_mutex_unlock(&queue->lock);

    return count;
}

/* ============================================================
 * VSOA DDS抽象层实现 (模拟实现)
 * 注意: 实际使用时需要替换为真实的VSOA库调用
 * ============================================================ */

/**
 * 初始化VSOA发布者
 * 注意: 这是一个模拟实现，实际需要使用libvsoa
 */
void *vsoa_publisher_init(const char *service_name) {
    /* 模拟实现: 创建假的发布者句柄 */
    /* 实际实现需要调用 VSOA Publisher API */

    static int mock_publisher_id = 1;
    int *publisher = (int *)malloc(sizeof(int));
    if (publisher != NULL) {
        *publisher = mock_publisher_id++;
        LOG("VSOA Publisher initialized: %s (id=%d)", service_name ? service_name : "unnamed", *publisher);
    }

    return publisher;
}

/**
 * 发布消息
 * 注意: 这是一个模拟实现
 */
int vsoa_publish(void *publisher, const char *topic, const void *data, int len) {
    if (publisher == NULL || data == NULL || len <= 0) {
        LOG_ERR("Invalid VSOA publish parameters");
        return -1;
    }

    /* 模拟实现: 只是打印日志 */
    int pub_id = *(int *)publisher;
    LOG("VSOA Publish [id=%d]: topic=%s, size=%d bytes", pub_id, topic ? topic : "unknown", len);

    /* 模拟: 实际这里应该调用 VSOA publisher_send 或类似API */
    /* 例如: vsoa_publisher_send(publisher, topic, data, len); */

    return 0;
}

/**
 * 关闭VSOA发布者
 */
void vsoa_publisher_close(void *publisher) {
    if (publisher != NULL) {
        int pub_id = *(int *)publisher;
        LOG("VSOA Publisher closed: id=%d", pub_id);
        free(publisher);
    }
}

/**
 * 初始化VSOA RPC服务器 (模拟实现)
 * 实际使用时需要调用真实的VSOA RPC API
 */
void *vsoa_rpc_server_init(const char *service_name) {
    static int mock_rpc_id = 1;
    int *rpc_server = (int *)malloc(sizeof(int));
    if (rpc_server != NULL) {
        *rpc_server = mock_rpc_id++;
        LOG("VSOA RPC Server initialized: %s (id=%d)", service_name ? service_name : "unnamed", *rpc_server);
    }
    return rpc_server;
}

/**
 * 关闭VSOA RPC服务器
 */
void vsoa_rpc_server_close(void *rpc_server) {
    if (rpc_server != NULL) {
        int rpc_id = *(int *)rpc_server;
        LOG("VSOA RPC Server closed: id=%d", rpc_id);
        free(rpc_server);
    }
}