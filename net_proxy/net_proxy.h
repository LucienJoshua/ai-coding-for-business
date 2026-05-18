/**
 * ============================================================
 * Net Proxy - 跨网段TCP/UDP数据转发代理服务器
 * ============================================================
 *
 * 功能说明:
 *   - 同时监听192.168和10.10.111两个网段
 *   - 接受来自任一网段的数据，解析目标IP后转发到10.10.111网段
 *   - 支持TCP和UDP协议
 *   - 使用epoll处理高并发连接
 *
 * 数据包格式:
 *   - 前16字节: 目标IP地址字符串 (如 "10.10.111.20")
 *   - 后续数据: 实际需要转发的内容
 *
 * 网络配置:
 *   - 192.168.3.10:8888 (TCP+UDP) - 监听A网段
 *   - 10.10.111.10:8889 (TCP+UDP) - 监听B网段
 *
 * 作者: Claude Code
 * 日期: 2026-04-27
 */

#ifndef NET_PROXY_H
#define NET_PROXY_H

/* ============================================================
 * 头文件引用
 * ============================================================ */
#include <stdio.h>      /* 标准输入输出: printf, fprintf */
#include <stdlib.h>     /* 标准库: malloc, free, exit */
#include <string.h>     /* 字符串操作: strncmp, memcpy, strncpy */
#include <unistd.h>     /* Unix标准函数: close, read, write */
#include <arpa/inet.h>  /* BSD socket: inet_pton, inet_ntop */
#include <netinet/in.h> /* 网络地址结构: sockaddr_in */
#include <sys/socket.h> /* socket API */
#include <sys/epoll.h>  /* epoll多路复用 */
#include <sys/fcntl.h>  /* 文件控制: fcntl (非阻塞设置) */
#include <pthread.h>    /* POSIX线程: pthread_mutex */
#include <errno.h>      /* 错误码: errno, strerror */
#include <time.h>       /* 时间函数: time, localtime, strftime */
#include <signal.h>     /* 信号处理: signal */

/* ============================================================
 * 常量定义
 * ============================================================ */

/* 最大并发事件数 - epoll一次能处理的最大事件数量 */
#define MAX_EVENTS 4096

/* 缓冲区大小 - 单次读写数据的上限 (64KB) */
#define BUFFER_SIZE 65536

/* 工作线程数 (预留，目前使用单线程事件驱动) */
#define MAX_THREADS 8

/* 目标网段前缀 - 用于判断目标IP是否属于10.10.111.x */
#define TARGET_NET "10.10.111"
#define TARGET_NET_PREFIX_LEN 9  /* "10.10.111"的长度 */

/* 监听端口 - 两个网段使用不同端口避免冲突 */
#define LISTEN_PORT_192 8888   /* 192.168网段监听端口 */
#define LISTEN_PORT_10  8889   /* 10.10.111网段监听端口 */

/* 调试模式开关 - 定义DEBUG时启用日志输出 */
#define DEBUG

/**
 * 日志宏 - 带时间戳的调试信息输出
 * 使用方法: LOG("Client connected: %s", client_ip);
 */
#ifdef DEBUG
#define LOG(fmt, ...) do { \
    time_t now = time(NULL); \
    char timebuf[32]; \
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&now)); \
    fprintf(stdout, "[%s] " fmt "\n", timebuf, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
#else
/* 发布版本不输出日志 */
#define LOG(fmt, ...) ((void)0)
#endif

/**
 * 错误日志宏 - 输出到stderr，不受DEBUG开关控制
 */
#define LOG_ERR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * 协议类型枚举
 * 用于区分TCP和UDP两种传输协议
 */
typedef enum {
    PROTOCOL_TCP,   /* 传输控制协议 - 面向连接，提供可靠传输 */
    PROTOCOL_UDP    /* 用户数据报协议 - 无连接，不保证可靠性 */
} protocol_type_t;

/**
 * 会话状态枚举
 * 跟踪每个代理会话的生命周期
 */
typedef enum {
    SESSION_STATE_INIT,     /* 初始化状态 - 刚创建会话 */
    SESSION_STATE_ACTIVE,   /* 活跃状态 - 正在传输数据 */
    SESSION_STATE_CLOSING   /* 关闭状态 - 准备清理 */
} session_state_t;

/**
 * 代理会话结构
 * 每个客户端连接对应一个会话，用于跟踪连接状态和管理资源
 *
 * 成员说明:
 *   - fd: 本地socket文件描述符，客户端连接服务器的socket
 *   - target_fd: 目标连接socket，服务器连接到目标IP的socket
 *   - protocol: 使用TCP还是UDP
 *   - state: 会话当前状态
 *   - client_ip: 客户端IP地址字符串
 *   - target_ip: 目标IP地址字符串(从数据中解析)
 *   - create_time: 会话创建时间，用于超时检测
 *   - last_active: 最后活跃时间，用于超时检测
 *   - next: 链表next指针，用于链接所有会话
 */
typedef struct proxy_session {
    int fd;                         /* 本地连接fd */
    int target_fd;                  /* 目标连接fd (TCP使用) */
    protocol_type_t protocol;       /* 协议类型 TCP/UDP */
    session_state_t state;          /* 会话状态 */
    char client_ip[INET_ADDRSTRLEN];/* 客户端IP "xxx.xxx.xxx.xxx" */
    char target_ip[INET_ADDRSTRLEN];/* 目标IP "xxx.xxx.xxx.xxx" */
    time_t create_time;             /* 创建时间戳 */
    time_t last_active;            /* 最后活跃时间戳 */
    struct proxy_session *next;     /* 链表next指针 */
} proxy_session_t;

/**
 * TCP长连接缓存条目
 * 为了支持频繁的TCP通信，缓存已建立的连接以便复用
 *
 * 成员说明:
 *   - target_ip: 目标服务器IP
 *   - target_fd: 已建立的连接socket
 *   - last_active: 最后使用时间
 *   - ref_count: 引用计数，多个请求可能共享同一连接
 *   - next: 链表next指针
 */
typedef struct tcp_conn_entry {
    char target_ip[INET_ADDRSTRLEN];/* 目标IP */
    int target_fd;                  /* 连接socket */
    time_t last_active;             /* 最后活跃时间 */
    int ref_count;                  /* 引用计数 */
    struct tcp_conn_entry *next;    /* 链表next */
} tcp_conn_entry_t;

/**
 * 全局配置结构
 * 存储程序运行所需的所有资源和状态
 *
 * 成员说明:
 *   - epoll_fd: epoll实例fd，用于I/O多路复用
 *   - tcp/udp_listen_fd_192: 192.168网段的监听socket
 *   - tcp/udp_listen_fd_10: 10.10.111网段的监听socket
 *   - session_lock: 保护会话链表的互斥锁
 *   - sessions: 活动会话链表头
 *   - session_count: 当前会话数量
 *   - conn_lock: 保护TCP连接缓存的互斥锁
 *   - tcp_conns: TCP长连接缓存链表
 */
typedef struct {
    int epoll_fd;                   /* epoll实例fd */
    int tcp_listen_fd_192;          /* 192.168网段TCP监听socket */
    int udp_listen_fd_192;          /* 192.168网段UDP监听socket */
    int tcp_listen_fd_10;           /* 10.10.111网段TCP监听socket */
    int udp_listen_fd_10;           /* 10.10.111网段UDP监听socket */
    pthread_mutex_t session_lock;    /* 会话链表互斥锁 */
    proxy_session_t *sessions;      /* 活动会话链表头 */
    int session_count;              /* 当前会话数量 */
    /* TCP长连接缓存 */
    pthread_mutex_t conn_lock;       /* 连接缓存互斥锁 */
    tcp_conn_entry_t *tcp_conns;    /* TCP长连接缓存链表 */
} global_config_t;

/* 全局配置变量，供所有模块访问 */
extern global_config_t g_config;

/* ============================================================
 * 函数声明
 * ============================================================ */

/* ---- 工具函数 ---- */

/**
 * 判断IP是否属于10.10.111网段
 * @param ip IP地址字符串 (如 "10.10.111.20")
 * @return 1 是目标网段, 0 不是目标网段
 *
 * 算法: 比较IP字符串的前9个字符是否等于"10.10.111"
 */
int is_target_network(const char *ip);

/**
 * 设置socket为非阻塞模式
 * @param fd socket文件描述符
 * @return 0成功, -1失败
 *
 * 为什么要非阻塞:
 *   - epoll配合非阻塞模式可以避免线程阻塞在单个I/O操作上
 *   - 提高并发处理能力
 */
int set_nonblocking(int fd);

/**
 * 根据fd获取绑定的本地IP
 * @param fd socket文件描述符
 * @return IP字符串，失败返回NULL
 *
 * 用途: 判断一个监听socket绑定的是哪个网段
 */
const char *get_bind_ip_by_fd(int fd);

/* ---- socket创建函数 ---- */

/**
 * 创建并绑定TCP监听socket
 * @param ip 要绑定的IP地址字符串，NULL表示绑定所有地址
 * @param port 端口号
 * @return socket fd，失败返回-1
 *
 * 流程:
 *   1. socket() 创建socket
 *   2. setsockopt() 设置SO_REUSEADDR允许地址复用
 *   3. bind() 绑定IP和端口
 *   4. listen() 开始监听
 */
int create_tcp_socket(const char *ip, int port);

/**
 * 创建并绑定UDP监听socket
 * @param ip 要绑定的IP地址，NULL表示绑定所有地址
 * @param port 端口号
 * @return socket fd，失败返回-1
 *
 * 流程:
 *   1. socket() 创建UDP socket
 *   2. bind() 绑定IP和端口
 *   (UDP不需要listen)
 */
int create_udp_socket(const char *ip, int port);

/* ---- 会话管理函数 ---- */

/**
 * 添加新会话到链表
 * @param fd 客户端连接socket
 * @param protocol TCP或UDP
 * @param client_ip 客户端IP地址
 * @param target_ip 目标IP地址(可为空)
 * @return 0成功, -1失败
 *
 * 线程安全: 使用session_lock保护
 */
int add_session(int fd, protocol_type_t protocol, const char *client_ip, const char *target_ip);

/**
 * 根据fd从链表中移除会话
 * @param fd 要移除的socket fd
 * @return 0成功, -1未找到
 *
 * 同时会关闭target_fd并释放内存
 */
int remove_session(int fd);

/**
 * 根据fd查找会话
 * @param fd socket文件描述符
 * @return 找到返回会话指针，未找到返回NULL
 */
proxy_session_t *find_session_by_fd(int fd);

/**
 * 清理超时会话
 * @param timeout_secs 超时秒数
 *
 * 遍历所有会话，移除超过timeout_secs未活跃的会话
 * 每60秒由主循环调用一次
 */
void cleanup_timeout_sessions(int timeout_secs);

/* ---- TCP长连接管理函数 ---- */

/**
 * 查找或创建到目标IP的TCP长连接
 * @param target_ip 目标服务器IP
 * @return 连接条目指针，失败返回NULL
 *
 * 流程:
 *   1. 先在缓存中查找是否存在可用连接
 *   2. 存在则增加引用计数并返回
 *   3. 不存在则创建新连接并存入缓存
 *
 * 线程安全: 使用conn_lock保护
 */
tcp_conn_entry_t *find_or_create_conn(const char *target_ip);

/**
 * 释放TCP连接引用
 * @param conn 连接条目指针
 *
 * 当引用计数减到0时，关闭socket并标记target_fd=-1
 * 实际连接会在cleanup_dead_conns()时释放内存
 */
void release_conn(tcp_conn_entry_t *conn);

/**
 * 清理无效的TCP连接
 *
 * 移除target_fd<0或ref_count<=0的条目
 * 由主循环定期调用
 */
void cleanup_dead_conns(void);

/* ---- 数据处理函数 ---- */

/**
 * 处理TCP数据转发
 * @param src_fd 源socket (客户端连接)
 * @param target_ip 目标IP (可为NULL，从数据中提取)
 * @param buffer 接收到的数据缓冲区
 * @param len 数据长度
 * @return 0成功, -1失败
 *
 * 数据格式: [16字节目标IP][实际数据]
 *
 * 流程:
 *   1. 解析前16字节获取目标IP
 *   2. 验证目标IP属于10.10.111网段
 *   3. 查找或创建到目标的长连接
 *   4. 发送数据(跳过前16字节)
 *   5. 接收响应
 *   6. 将响应返回给客户端
 *   7. 释放连接引用(长连接保留供下次使用)
 */
int forward_tcp_to_target(int src_fd, const char *target_ip, char *buffer, int len);

/**
 * 处理UDP数据转发
 * @param target_ip 目标IP (可为NULL，从数据中提取)
 * @param buffer 接收到的数据缓冲区
 * @param len 数据长度
 * @return 0成功, -1失败
 *
 * 数据格式: [16字节目标IP][实际数据]
 *
 * 流程:
 *   1. 解析前16字节获取目标IP
 *   2. 验证目标IP属于10.10.111网段
 *   3. 创建临时UDP socket
 *   4. 发送数据(跳过前16字节)
 *   5. 关闭临时socket
 *
 * 注意: UDP不需要响应，发送完就结束
 */
int forward_udp_to_target(const char *target_ip, char *buffer, int len);

/**
 * 处理TCP客户端数据
 * @param fd 客户端socket fd
 *
 * 从fd读取数据，调用forward_tcp_to_target转发
 * 处理连接关闭和错误情况
 */
void handle_tcp_data(int fd);

/**
 * 处理UDP数据
 * @param fd UDP监听socket
 * @param client_addr 客户端地址信息
 * @param addr_len 地址结构长度
 *
 * 从fd读取UDP数据，调用forward_udp_to_target转发
 */
void handle_udp_data(int fd, struct sockaddr_in *client_addr, socklen_t addr_len);

/* ---- 事件处理函数 ---- */

/**
 * 初始化所有监听socket
 * @return 0成功, -1失败
 *
 * 创建4个监听socket:
 *   - 192.168网段: TCP和UDP各一个
 *   - 10.10.111网段: TCP和UDP各一个
 *
 * 并将它们全部加入到epoll实例中监听
 */
int init_listen_sockets(void);

/**
 * 主事件循环
 *
 * 使用epoll_wait阻塞等待事件，事件到达后分发处理:
 *   - TCP监听socket: 调用handle_tcp_accept接受新连接
 *   - UDP监听socket: 调用handle_udp_data处理数据
 *   - 普通TCP socket: 调用handle_tcp_data处理数据
 *   - 错误/关闭事件: 关闭socket并清理会话
 *
 * 每60秒调用一次cleanup清理超时会话
 */
void run_event_loop(void);

#endif /* NET_PROXY_H */