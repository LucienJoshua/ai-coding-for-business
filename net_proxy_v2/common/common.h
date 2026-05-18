/**
 * ============================================================
 * Net Proxy V2 - 跨网段数据转发代理服务器 VSOA版
 * ============================================================
 *
 * 功能说明:
 *   - 程序1和程序2是同一个程序，部署在不同网段的机器上
 *   - 程序1部署在192.168网段，接收client_a数据并转发给程序2
 *   - 程序2部署在10.10.111网段，接收程序1数据并通过VSOA发布给subscriber_b
 *   - 支持内部消息队列处理高并发
 *   - 支持TCP和UDP协议
 *
 * 通信架构:
 *   [client_a] --> [程序1] --> [程序2] --> [subscriber_b]
 *                 (192.168网段)   (10.10.111网段)
 *
 * 数据格式 (自定义简单格式):
 *   - 协议头(8字节): [魔数2字节][版本1字节][类型1字节][保留4字节]
 *   - 消息头(32字节): [序列号4字节][时间戳8字节][源IP16字节]
 *   - 负载数据: 可变长度
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#ifndef COMMON_H
#define COMMON_H

/* ============================================================
 * 头文件引用
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

/* 协议魔数 - 用于数据包识别 */
#define PROTOCOL_MAGIC 0x56504159  /* "VPAY" */

/* 协议版本 */
#define PROTOCOL_VERSION 0x01

/* 消息类型 */
#define MSG_TYPE_TEXT     0x01  /* 文本数据 */
#define MSG_TYPE_BINARY   0x02  /* 二进制数据 */
#define MSG_TYPE_HEARTBEAT 0x03 /* 心跳消息 */
#define MSG_TYPE_ACK      0x04  /* 确认消息 */
#define MSG_TYPE_TCP      0x05   /* TCP转发标记 */
#define MSG_TYPE_UDP      0x06   /* UDP转发标记 */

/* 最大缓冲区大小 */
#define MAX_BUFFER_SIZE 65536

/* 消息队列容量 */
#define MSG_QUEUE_SIZE 10240

/* 最大并发事件数 */
#define MAX_EVENTS 4096

/* 目标网段前缀 */
#define TARGET_NET "10.10.111"
#define TARGET_NET_PREFIX_LEN 9

/* 监听端口配置 */
#define LISTEN_PORT_192 8888   /* 192.168网段监听端口 */
#define LISTEN_PORT_10  8889   /* 10.10.111网段监听端口 */

/* VSOA相关配置 - 可根据实际情况修改 */
#define VSOA_SERVICE_NAME "net_proxy_dds"
#define VSOA_TOPIC_DATA "proxy_data_topic"
#define VSOA_TOPIC_CTRL "proxy_ctrl_topic"

/* 调试模式 */
#define DEBUG

#ifdef DEBUG
#define LOG(fmt, ...) do { \
    time_t now = time(NULL); \
    char timebuf[32]; \
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now)); \
    fprintf(stdout, "[%s] " fmt "\n", timebuf, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
#define LOG(fmt, ...) ((void)0)
#endif

#define LOG_ERR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

/* ============================================================
 * 数据结构定义
 * ============================================================ */

/**
 * 协议头结构 (8字节)
 * 每个数据包都以这个头开始
 */
typedef struct __attribute__((packed)) {
    uint16_t magic;      /* 魔数 PROTOCOL_MAGIC */
    uint8_t  version;   /* 协议版本 */
    uint8_t  msg_type;  /* 消息类型 */
    uint32_t reserved;  /* 保留字段 */
} protocol_header_t;

/**
 * 消息头结构 (32字节)
 * 紧跟在协议头后面
 */
typedef struct __attribute__((packed)) {
    uint32_t seq_num;       /* 序列号 */
    uint64_t timestamp;    /* 时间戳 */
    char src_ip[16];       /* 源IP地址字符串 */
} message_header_t;

/**
 * 完整消息结构
 * 包含协议头、消息头和负载数据
 */
typedef struct {
    protocol_header_t header;      /* 8字节协议头 */
    message_header_t msg_header;   /* 32字节消息头 */
    char *payload;                 /* 负载数据指针 */
    int payload_len;              /* 负载数据长度 */
    int total_len;                 /* 完整消息长度 */
} message_t;

/**
 * 消息节点结构 (用于消息队列)
 */
typedef struct msg_queue_node {
    message_t msg;
    struct msg_queue_node *next;
} msg_queue_node_t;

/**
 * 线程安全的消息队列
 */
typedef struct {
    msg_queue_node_t *front;       /* 队列头部 */
    msg_queue_node_t *rear;        /* 队列尾部 */
    int count;                     /* 当前消息数量 */
    int capacity;                  /* 队列容量 */
    pthread_mutex_t lock;          /* 互斥锁 */
    pthread_cond_t not_empty;      /* 非空条件变量 */
    pthread_cond_t not_full;       /* 非满条件变量 */
    bool shutdown;                 /* 关闭标志 */
} msg_queue_t;

/**
 * 协议类型枚举
 */
typedef enum {
    PROTOCOL_TCP,
    PROTOCOL_UDP
} protocol_type_t;

/**
 * 会话状态枚举
 */
typedef enum {
    SESSION_STATE_INIT,
    SESSION_STATE_ACTIVE,
    SESSION_STATE_CLOSING
} session_state_t;

/**
 * 代理会话结构
 */
typedef struct proxy_session {
    int fd;
    int target_fd;
    protocol_type_t protocol;
    session_state_t state;
    char client_ip[INET_ADDRSTRLEN];
    time_t create_time;
    time_t last_active;
    struct proxy_session *next;
} proxy_session_t;

/**
 * 全局配置结构
 */
typedef struct {
    int epoll_fd;
    int tcp_listen_fd_192;
    int udp_listen_fd_192;
    int tcp_listen_fd_10;
    int udp_listen_fd_10;
    pthread_mutex_t session_lock;
    proxy_session_t *sessions;
    int session_count;
    /* 消息队列 */
    msg_queue_t *msg_queue;
    /* VSOA发布者 */
    void *vsoa_publisher;
    void *vsoa_rpc_server;  /* VSOA RPC服务器 - 接收来自client_a的RPC调用 */
    bool vsoa_enabled;
    /* 程序角色: true=程序1(192.168网段), false=程序2(10.10.111网段) */
    bool is_proxy1;
} global_config_t;

extern global_config_t g_config;

/* ============================================================
 * 函数声明 - 工具函数
 * ============================================================ */

/**
 * 判断IP是否属于10.10.111网段
 * @param ip IP地址字符串
 * @return 1 是目标网段, 0 不是
 */
int is_target_network(const char *ip);

/**
 * 设置socket为非阻塞模式
 * @param fd socket文件描述符
 * @return 0成功, -1失败
 */
int set_nonblocking(int fd);

/**
 * 获取当前时间戳(毫秒)
 * @return 当前时间戳
 */
uint64_t get_timestamp_ms(void);

/**
 * 创建消息结构
 * @param type 消息类型
 * @param payload 负载数据
 * @param payload_len 负载长度
 * @param src_ip 源IP
 * @return 消息结构指针，失败返回NULL
 */
message_t *create_message(uint8_t type, const char *payload, int payload_len, const char *src_ip);

/**
 * 释放消息结构
 * @param msg 消息指针
 */
void free_message(message_t *msg);

/**
 * 序列化消息到缓冲区
 * @param msg 消息指针
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数，失败返回-1
 */
int serialize_message(const message_t *msg, char *buffer, int buffer_size);

/**
 * 从缓冲区解析消息
 * @param buffer 数据缓冲区
 * @param len 数据长度
 * @return 消息结构指针，失败返回NULL
 */
message_t *deserialize_message(const char *buffer, int len);

/* ============================================================
 * 函数声明 - 消息队列
 * ============================================================ */

/**
 * 初始化消息队列
 * @param capacity 队列容量
 * @return 队列指针，失败返回NULL
 */
msg_queue_t *msg_queue_init(int capacity);

/**
 * 销毁消息队列
 * @param queue 队列指针
 */
void msg_queue_destroy(msg_queue_t *queue);

/**
 * 入队消息(阻塞直到成功或队列关闭)
 * @param queue 队列指针
 * @param msg 消息指针
 * @return 0成功, -1队列已关闭
 */
int msg_queue_push(msg_queue_t *queue, message_t *msg);

/**
 * 出队消息(阻塞直到有消息或队列关闭)
 * @param queue 队列指针
 * @param timeout_ms 超时毫秒，-1表示无限等待
 * @return 消息指针，超时或关闭返回NULL
 */
message_t *msg_queue_pop(msg_queue_t *queue, int timeout_ms);

/**
 * 获取队列当前消息数
 * @param queue 队列指针
 * @return 消息数量
 */
int msg_queue_count(msg_queue_t *queue);

/* ============================================================
 * 函数声明 - VSOA DDS抽象层
 * ============================================================ */

/**
 * 初始化VSOA发布者
 * @param service_name 服务名称
 * @return 出版者句柄，失败返回NULL
 */
void *vsoa_publisher_init(const char *service_name);

/**
 * 发布消息
 * @param publisher 出版者句柄
 * @param topic 主题名称
 * @param data 数据指针
 * @param len 数据长度
 * @return 0成功, -1失败
 */
int vsoa_publish(void *publisher, const char *topic, const void *data, int len);

/**
 * 关闭VSOA发布者
 * @param publisher 出版者句柄
 */
void vsoa_publisher_close(void *publisher);

/**
 * 初始化VSOA RPC服务器
 * @param service_name 服务名称
 * @return RPC服务器句柄，失败返回NULL
 */
void *vsoa_rpc_server_init(const char *service_name);

/**
 * 关闭VSOA RPC服务器
 * @param rpc_server RPC服务器句柄
 */
void vsoa_rpc_server_close(void *rpc_server);

#endif /* COMMON_H */