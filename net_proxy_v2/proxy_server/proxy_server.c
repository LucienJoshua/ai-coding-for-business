/**
 * ============================================================
 * Proxy Server - 跨网段数据转发代理服务器 (统一版本)
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
 *                                  |                       |
 *                             消息队列                 消息队列
 *
 * 使用方法:
 *   启动命令: ./proxy_server [目标IP]
 *   例如:
 *     在192.168网段机器: ./proxy_server 10.10.111.20
 *     在10.10.111网段机器: ./proxy_server 192.168.x.x
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#include "proxy_server.h"
#include <sys/time.h>

/* ============================================================
 * 全局配置
 * ============================================================ */

/**
 * 全局配置实例
 * 包含所有监听socket、消息队列、VSOA句柄等
 */
global_config_t g_config = {
    .epoll_fd = -1,
    .tcp_listen_fd_192 = -1,
    .udp_listen_fd_192 = -1,
    .tcp_listen_fd_10 = -1,
    .udp_listen_fd_10 = -1,
    .sessions = NULL,
    .session_count = 0,
    .msg_queue = NULL,
    .vsoa_publisher = NULL,
    .vsoa_rpc_server = NULL,
    .vsoa_enabled = false,
    .is_proxy1 = true
};

/* 目标IP配置 (对端程序的IP) */
static char g_target_ip[INET_ADDRSTRLEN] = "10.10.111.20";
/* 本地监听IP地址 */
static const char *LOCAL_IP_192 = "192.168.3.10";
static const char *LOCAL_IP_10 = "10.10.111.10";
/* 监听端口 */
static const int LISTEN_PORT_PROXY = 8888;

/* ============================================================
 * 工具函数实现 (来自common.c)
 * ============================================================ */

/* ============================================================
 * Socket创建函数实现
 * ============================================================ */

/**
 * 创建TCP监听socket
 */
int create_tcp_socket(const char *ip, int port) {
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERR("TCP socket create failed: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERR("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip == NULL) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip, &addr.sin_addr) < 0) {
            LOG_ERR("inet_pton failed for IP: %s", ip);
            close(fd);
            return -1;
        }
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("bind to %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        LOG_ERR("listen on %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    LOG("TCP socket listening on %s:%d (fd=%d)", ip ? ip : "0.0.0.0", port, fd);
    return fd;
}

/**
 * 创建UDP监听socket
 */
int create_udp_socket(const char *ip, int port) {
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERR("UDP socket create failed: %s", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (ip == NULL) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, ip, &addr.sin_addr) < 0) {
            LOG_ERR("inet_pton failed for IP: %s", ip);
            close(fd);
            return -1;
        }
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("bind to %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    LOG("UDP socket listening on %s:%d (fd=%d)", ip ? ip : "0.0.0.0", port, fd);
    return fd;
}

/* ============================================================
 * VSOA RPC回调和数据处理
 * ============================================================ */

/**
 * VSOA RPC数据回调 (模拟实现)
 * 当收到client_a的VSOA RPC调用时，会调用此函数
 *
 * @param data 收到的数据
 * @param len 数据长度
 * @return 0成功处理, -1失败
 */
int vsoa_rpc_data_callback(const char *data, int len) {
    if (data == NULL || len <= 0) {
        LOG_ERR("Invalid RPC data");
        return -1;
    }

    LOG("VSOA RPC received: %d bytes", len);

    /* 解析数据格式: [协议头(8)][消息头(32)][目标IP(16)][负载数据] */
    if (len < sizeof(protocol_header_t) + sizeof(message_header_t) + 16) {
        LOG_ERR("Data too short for parsing");
        return -1;
    }

    /* 跳过协议头和消息头，直接获取目标IP */
    const char *dest_ip_ptr = data + sizeof(protocol_header_t) + sizeof(message_header_t);
    char dest_ip[16];
    strncpy(dest_ip, dest_ip_ptr, 15);
    dest_ip[15] = '\0';

    /* 获取负载数据 */
    const char *payload = dest_ip_ptr + 16;
    int payload_len = len - (sizeof(protocol_header_t) + sizeof(message_header_t) + 16);

    LOG("Parsed from RPC: dest_ip=%s, payload_len=%d", dest_ip, payload_len);

    /* 验证目标IP是否在10.10.111网段 */
    if (!is_target_network(dest_ip)) {
        LOG_ERR("Destination IP %s is not in target network %s", dest_ip, TARGET_NET);
        return -1;
    }

    /* 将数据放入消息队列，由工作线程转发 */
    message_t *msg = create_message(MSG_TYPE_TEXT, payload, payload_len, LOCAL_IP_192);
    if (msg == NULL) {
        LOG_ERR("Failed to create message for queue");
        return -1;
    }

    /* 保存目标IP到消息中(在负载前16字节) */
    msg->payload = malloc(payload_len + 16);
    if (msg->payload == NULL) {
        free_message(msg);
        return -1;
    }
    memset(msg->payload, 0, 16);
    strncpy(msg->payload, dest_ip, 15);
    memcpy(msg->payload + 16, payload, payload_len);
    msg->payload_len = payload_len + 16;

    if (msg_queue_push(g_config.msg_queue, msg) < 0) {
        free_message(msg);
        return -1;
    }

    LOG("RPC data queued for forwarding to %s", dest_ip);
    return 0;
}

/**
 * 模拟VSOA RPC接收 (在实际环境中通过回调触发)
 * 此函数在模拟环境中被周期性调用
 */
void check_vsoa_rpc(void) {
    /* 模拟实现: 实际VSOA会通过回调函数接收数据 */
    /* 这里仅作演示，实际不会收到数据 */
}

/* ============================================================
 * 消息队列处理
 * ============================================================ */

/**
 * 消息队列工作线程
 * 从队列取出消息，根据数据类型通过TCP或UDP发送到目标IP
 */
void *queue_worker_thread(void *arg) {
    (void)arg;
    LOG("Queue worker thread started");

    while (1) {
        /* 从队列取出消息 */
        message_t *msg = msg_queue_pop(g_config.msg_queue, 1000);
        if (msg == NULL) {
            /* 超时或队列关闭 */
            continue;
        }

        /* 解析目标IP (前16字节) */
        char dest_ip[16];
        memset(dest_ip, 0, sizeof(dest_ip));
        if (msg->payload_len >= 16) {
            strncpy(dest_ip, msg->payload, 15);
        }

        /* 获取实际负载数据 */
        char *actual_payload = msg->payload + 16;
        int actual_len = msg->payload_len - 16;

        if (actual_len <= 0) {
            LOG_ERR("Invalid payload length: %d", actual_len);
            free_message(msg);
            continue;
        }

        LOG("Forwarding to %s: %d bytes (type=%d)", dest_ip, actual_len, msg->header.msg_type);

        /* 根据消息类型选择协议:
         * - TEXT和ACK使用TCP(可靠传输)
         * - BINARY和HEARTBEAT使用UDP(高效传输)
         */
        if (msg->header.msg_type == MSG_TYPE_TCP) {
            /* TCP转发 - 建立连接并发送 */
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd >= 0) {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(LISTEN_PORT_PROXY);
                inet_pton(AF_INET, dest_ip, &addr.sin_addr);

                if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                    send(fd, actual_payload, actual_len, 0);
                    LOG("TCP forwarded to %s:%d", dest_ip, LISTEN_PORT_PROXY);
                } else {
                    LOG_ERR("TCP connect to %s failed: %s", dest_ip, strerror(errno));
                }
                close(fd);
            }
        } else {
            /* UDP转发 - 直接发送 */
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd >= 0) {
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(LISTEN_PORT_PROXY);
                inet_pton(AF_INET, dest_ip, &addr.sin_addr);

                sendto(fd, actual_payload, actual_len, 0, (struct sockaddr *)&addr, sizeof(addr));
                LOG("UDP forwarded to %s:%d", dest_ip, LISTEN_PORT_PROXY);
                close(fd);
            }
        }

        free_message(msg);
    }

    return NULL;
}

/* ============================================================
 * TCP/UDP数据处理
 * ============================================================ */

/**
 * 处理TCP连接接受
 */
void handle_tcp_accept(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (client_fd < 0) {
        LOG_ERR("accept failed: %s", strerror(errno));
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    LOG("TCP connection from %s:%d (fd=%d)", client_ip, ntohs(client_addr.sin_port), client_fd);

    /* 设置非阻塞 */
    set_nonblocking(client_fd);

    /* 添加到epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = client_fd;
    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        LOG_ERR("epoll_ctl add TCP client failed");
        close(client_fd);
        return;
    }
}

/**
 * 处理TCP数据
 * 如果数据来自另一个proxy_server实例，则VSOA发布给subscriber_b
 */
void handle_tcp_data(int fd) {
    char buffer[MAX_BUFFER_SIZE];
    int len = recv(fd, buffer, sizeof(buffer), 0);

    if (len <= 0) {
        if (len < 0) {
            LOG_ERR("recv TCP data failed: %s", strerror(errno));
        } else {
            LOG("TCP connection closed");
        }
        epoll_ctl(g_config.epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        return;
    }

    LOG("TCP received %d bytes, VSOA publishing...", len);

    /* 来自另一个proxy_server的TCP数据，通过VSOA发布给subscriber_b */
    if (g_config.vsoa_enabled && g_config.vsoa_publisher) {
        vsoa_publish(g_config.vsoa_publisher, VSOA_TOPIC_DATA, buffer, len);
        LOG("Data published via VSOA to subscriber_b");
    }
}

/**
 * 处理UDP数据
 * 如果数据来自另一个proxy_server实例，则VSOA发布给subscriber_b
 */
void handle_udp_data(int fd, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char buffer[MAX_BUFFER_SIZE];
    int len = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)client_addr, &addr_len);

    if (len <= 0) {
        if (len < 0) {
            LOG_ERR("recv UDP data failed: %s", strerror(errno));
        }
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));

    LOG("UDP received %d bytes from %s, VSOA publishing...", len, client_ip);

    /* 来自另一个proxy_server的UDP数据，通过VSOA发布给subscriber_b */
    if (g_config.vsoa_enabled && g_config.vsoa_publisher) {
        vsoa_publish(g_config.vsoa_publisher, VSOA_TOPIC_DATA, buffer, len);
        LOG("Data published via VSOA to subscriber_b");
    }
}

/* ============================================================
 * 初始化和事件循环
 * ============================================================ */

/**
 * 初始化监听socket
 * 同时监听192.168和10.10.111两个网段
 */
int init_listen_sockets(void) {
    LOG("Initializing listening sockets...");

    /* 在192.168网段创建TCP监听 */
    g_config.tcp_listen_fd_192 = create_tcp_socket(LOCAL_IP_192, LISTEN_PORT_PROXY);
    if (g_config.tcp_listen_fd_192 < 0) {
        LOG_ERR("Failed to create TCP socket on %s", LOCAL_IP_192);
        return -1;
    }
    set_nonblocking(g_config.tcp_listen_fd_192);

    /* 在192.168网段创建UDP监听 */
    g_config.udp_listen_fd_192 = create_udp_socket(LOCAL_IP_192, LISTEN_PORT_PROXY);
    if (g_config.udp_listen_fd_192 < 0) {
        LOG_ERR("Failed to create UDP socket on %s", LOCAL_IP_192);
        return -1;
    }

    /* 在10.10.111网段创建TCP监听 */
    g_config.tcp_listen_fd_10 = create_tcp_socket(LOCAL_IP_10, LISTEN_PORT_PROXY);
    if (g_config.tcp_listen_fd_10 < 0) {
        LOG_ERR("Failed to create TCP socket on %s", LOCAL_IP_10);
        return -1;
    }
    set_nonblocking(g_config.tcp_listen_fd_10);

    /* 在10.10.111网段创建UDP监听 */
    g_config.udp_listen_fd_10 = create_udp_socket(LOCAL_IP_10, LISTEN_PORT_PROXY);
    if (g_config.udp_listen_fd_10 < 0) {
        LOG_ERR("Failed to create UDP socket on %s", LOCAL_IP_10);
        return -1;
    }

    LOG("All listening sockets initialized successfully");
    return 0;
}

/**
 * 初始化代理服务器
 */
int proxy_init(void) {
    LOG("Initializing proxy server...");

    /* 初始化消息队列 */
    g_config.msg_queue = msg_queue_init(MSG_QUEUE_SIZE);
    if (g_config.msg_queue == NULL) {
        LOG_ERR("Failed to initialize message queue");
        return -1;
    }

    /* 初始化VSOA发布者 - 用于向subscriber_b发布数据 */
    g_config.vsoa_publisher = vsoa_publisher_init(VSOA_SERVICE_NAME);
    if (g_config.vsoa_publisher != NULL) {
        g_config.vsoa_enabled = true;
        LOG("VSOA publisher enabled");
    } else {
        LOG("VSOA publisher init failed, continuing without VSOA");
        g_config.vsoa_enabled = false;
    }

    /* 初始化VSOA RPC服务器 - 用于接收client_a的RPC调用 */
    g_config.vsoa_rpc_server = vsoa_rpc_server_init(VSOA_SERVICE_NAME "_rpc");
    if (g_config.vsoa_rpc_server != NULL) {
        LOG("VSOA RPC server initialized");
    }

    /* 初始化会话链表锁 */
    pthread_mutex_init(&g_config.session_lock, NULL);

    LOG("Proxy server initialized successfully");
    return 0;
}

/**
 * 清理代理服务器资源
 */
void proxy_cleanup(void) {
    LOG("Cleaning up proxy server...");

    /* 关闭监听socket */
    if (g_config.tcp_listen_fd_192 >= 0) close(g_config.tcp_listen_fd_192);
    if (g_config.udp_listen_fd_192 >= 0) close(g_config.udp_listen_fd_192);
    if (g_config.tcp_listen_fd_10 >= 0) close(g_config.tcp_listen_fd_10);
    if (g_config.udp_listen_fd_10 >= 0) close(g_config.udp_listen_fd_10);

    /* 关闭epoll */
    if (g_config.epoll_fd >= 0) close(g_config.epoll_fd);

    /* 销毁VSOA */
    if (g_config.vsoa_publisher) vsoa_publisher_close(g_config.vsoa_publisher);
    if (g_config.vsoa_rpc_server) vsoa_rpc_server_close(g_config.vsoa_rpc_server);

    /* 销毁消息队列 */
    if (g_config.msg_queue) msg_queue_destroy(g_config.msg_queue);

    /* 销毁会话链表锁 */
    pthread_mutex_destroy(&g_config.session_lock);

    LOG("Proxy server cleanup completed");
}

/**
 * 主事件循环
 */
void run_event_loop(void) {
    LOG("Starting event loop...");

    /* 创建epoll实例 */
    g_config.epoll_fd = epoll_create1(0);
    if (g_config.epoll_fd < 0) {
        LOG_ERR("epoll_create1 failed: %s", strerror(errno));
        return;
    }

    /* 注册TCP监听socket到epoll */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;

    ev.data.fd = g_config.tcp_listen_fd_192;
    epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.tcp_listen_fd_192, &ev);

    ev.data.fd = g_config.tcp_listen_fd_10;
    epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.tcp_listen_fd_10, &ev);

    /* 注册UDP监听socket到epoll (不使用ET模式) */
    ev.events = EPOLLIN;
    ev.data.fd = g_config.udp_listen_fd_192;
    epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.udp_listen_fd_192, &ev);

    ev.data.fd = g_config.udp_listen_fd_10;
    epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.udp_listen_fd_10, &ev);

    /* 事件数组 */
    struct epoll_event events[MAX_EVENTS];

    /* 主循环 */
    while (1) {
        int nfds = epoll_wait(g_config.epoll_fd, events, MAX_EVENTS, 100);

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            /* 判断是哪个socket的事件 */
            if (fd == g_config.tcp_listen_fd_192 || fd == g_config.tcp_listen_fd_10) {
                /* TCP监听socket - 有新连接 */
                if (events[i].events & EPOLLIN) {
                    handle_tcp_accept(fd);
                }
            } else if (fd == g_config.udp_listen_fd_192 || fd == g_config.udp_listen_fd_10) {
                /* UDP监听socket - 有新数据 */
                if (events[i].events & EPOLLIN) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    handle_udp_data(fd, &client_addr, addr_len);
                }
            } else {
                /* TCP客户端socket - 有数据到达 */
                if (events[i].events & EPOLLIN) {
                    handle_tcp_data(fd);
                }
            }
        }

        /* 检查VSOA RPC (模拟环境) */
        check_vsoa_rpc();
    }
}

/* ============================================================
 * 主函数
 * ============================================================ */

static void signal_handler(int sig) {
    (void)sig;
    LOG("Received signal, shutting down...");
    proxy_cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    printf("=========================================\n");
    printf("    Proxy Server - Unified Version\n");
    printf("=========================================\n");
    printf("Usage: ./proxy_server [target_ip]\n");
    printf("Example: ./proxy_server 10.10.111.20\n");
    printf("=========================================\n\n");

    /* 解析命令行参数 */
    if (argc >= 2) {
        strncpy(g_target_ip, argv[1], INET_ADDRSTRLEN - 1);
        g_target_ip[INET_ADDRSTRLEN - 1] = '\0';
        LOG("Target IP set to: %s", g_target_ip);
    } else {
        LOG("Using default target IP: %s", g_target_ip);
    }

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 初始化 */
    if (proxy_init() < 0) {
        LOG_ERR("Proxy initialization failed");
        return 1;
    }

    /* 初始化监听socket */
    if (init_listen_sockets() < 0) {
        LOG_ERR("Socket initialization failed");
        proxy_cleanup();
        return 1;
    }

    /* 启动消息队列工作线程 */
    pthread_t worker_tid;
    if (pthread_create(&worker_tid, NULL, queue_worker_thread, NULL) != 0) {
        LOG_ERR("Failed to create queue worker thread");
        proxy_cleanup();
        return 1;
    }
    pthread_detach(worker_tid);

    LOG("=========================================");
    LOG("    Proxy Server Running");
    LOG("=========================================");
    LOG("Listening on:");
    LOG("  - TCP/UDP %s:%d (for client_a and proxy)", LOCAL_IP_192, LISTEN_PORT_PROXY);
    LOG("  - TCP/UDP %s:%d (for proxy and subscriber)", LOCAL_IP_10, LISTEN_PORT_PROXY);
    LOG("VSOA:");
    LOG("  - RPC server for receiving from client_a");
    LOG("  - Publisher for sending to subscriber_b");
    LOG("=========================================");
    LOG("Ready to forward data between networks");
    LOG("Press Ctrl+C to exit");
    LOG("=========================================\n");

    /* 运行事件循环 */
    run_event_loop();

    proxy_cleanup();
    return 0;
}
