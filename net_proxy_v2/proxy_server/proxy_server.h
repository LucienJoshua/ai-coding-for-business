/**
 * ============================================================
 * Proxy Server - 跨网段数据转发代理服务器
 * ============================================================
 *
 * 功能说明:
 *   - 程序1和程序2是同一个程序，使用相同启动方式
 *   - 程序1(192.168网段): 接收client_a的VSOA RPC调用，转发TCP/UDP到Program 2
 *   - 程序2(10.10.111网段): 接收TCP/UDP数据，VSOA Publish到subscriber_b
 *   - 支持双向数据流
 *
 * 通信架构:
 *   [client_a] --VSOA RPC--> [Program 1] --TCP/UDP--> [Program 2] --VSOA Publish--> [subscriber_b]
 *
 * 使用方法:
 *   ./proxy_server [目标IP]
 *   例如:
 *     在192.168网段机器: ./proxy_server 10.10.111.20
 *     在10.10.111网段机器: ./proxy_server 192.168.x.x
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include "../common/common.h"

/* ============================================================
 * 函数声明
 * ============================================================ */

/* ---- 初始化和清理 ---- */

/**
 * 初始化代理服务器
 * @return 0成功, -1失败
 */
int proxy_init(void);

/**
 * 清理代理服务器资源
 */
void proxy_cleanup(void);

/* ---- socket创建 ---- */

/**
 * 创建TCP监听socket
 */
int create_tcp_socket(const char *ip, int port);

/**
 * 创建UDP监听socket
 */
int create_udp_socket(const char *ip, int port);

/* ---- 数据处理 ---- */

/**
 * 处理TCP接受
 */
void handle_tcp_accept(int listen_fd);

/**
 * 处理TCP数据
 */
void handle_tcp_data(int fd);

/**
 * 处理UDP数据
 */
void handle_udp_data(int fd, struct sockaddr_in *client_addr, socklen_t addr_len);

/**
 * 队列处理线程 - 从队列取出消息并转发
 */
void *queue_worker_thread(void *arg);

/* ---- VSOA RPC ---- */

/**
 * VSOA RPC数据回调
 */
int vsoa_rpc_data_callback(const char *data, int len);

/* ---- 事件循环 ---- */

/**
 * 初始化监听socket
 */
int init_listen_sockets(void);

/**
 * 主事件循环
 */
void run_event_loop(void);

#endif /* PROXY_SERVER_H */
