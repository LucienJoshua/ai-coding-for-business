/**
 * ============================================================
 * Net Proxy - 主程序实现
 * ============================================================
 *
 * 作者: Claude Code
 * 日期: 2026-04-27
 */

#include "net_proxy.h"

/* ============================================================
 * 全局变量
 * ============================================================ */

/**
 * 全局配置实例
 * 所有监听socket、会话链表、连接缓存都在这里
 * 初始化为全-1/NULL表示未初始化状态
 */
global_config_t g_config = {
    .epoll_fd = -1,
    .tcp_listen_fd_192 = -1,
    .udp_listen_fd_192 = -1,
    .tcp_listen_fd_10 = -1,
    .udp_listen_fd_10 = -1,
    .sessions = NULL,
    .session_count = 0,
    .tcp_conns = NULL
};

/* ============================================================
 * 工具函数实现
 * ============================================================ */

/**
 * 判断IP是否属于10.10.111网段
 *
 * 算法:
 *   使用strncmp比较IP字符串的前9个字符
 *   "10.10.111"刚好是这个网段的共同前缀
 *
 * 示例:
 *   "10.10.111.20" -> 前9字符是"10.10.111" -> 匹配 -> 是目标网段
 *   "192.168.3.10" -> 前9字符是"192.168.3" -> 不匹配 -> 不是目标网段
 *
 * @param ip IP地址字符串
 * @return 1 是目标网段, 0 不是目标网段或输入为NULL
 */
int is_target_network(const char *ip) {
    /* 防御性检查：防止NULL指针导致崩溃 */
    if (ip == NULL) {
        return 0;
    }
    /* 比较前9个字符是否等于"10.10.111" */
    return strncmp(ip, TARGET_NET, TARGET_NET_PREFIX_LEN) == 0;
}

/**
 * 设置socket为非阻塞模式
 *
 * 为什么要非阻塞:
 *   - 默认的阻塞模式会让read/write调用等待数据或对方确认
 *   - 非阻塞模式下，数据没到达时立即返回-1，errno=EAGAIN
 *   - 这样配合epoll可以同时处理大量连接，不会卡在某个连接上
 *
 * @param fd socket文件描述符
 * @return 0成功, -1失败
 */
int set_nonblocking(int fd) {
    /* 获取当前flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    /* 添加O_NONBLOCK标志，原flags保留其他属性 */
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * 根据fd获取绑定的本地IP
 *
 * 用途:
 *   当收到新连接时，判断监听socket绑定的是哪个IP
 *   从而知道这个连接属于哪个网段(192.168还是10.10.111)
 *
 * @param fd socket文件描述符
 * @return IP字符串，失败返回NULL
 */
const char *get_bind_ip_by_fd(int fd) {
    static char ip[INET_ADDRSTRLEN];  /* static避免栈内存问题 */
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    /* getsockname获取socket绑定的地址 */
    if (getsockname(fd, (struct sockaddr *)&addr, &addr_len) < 0) {
        return NULL;
    }

    /* 将二进制IP转为点分十进制字符串 */
    if (inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)) == NULL) {
        return NULL;
    }

    return ip;
}

/* ============================================================
 * Socket创建函数实现
 * ============================================================ */

/**
 * 创建并绑定TCP监听socket
 *
 * 完整的socket创建流程:
 *   socket() -> setsockopt() -> bind() -> listen()
 *
 * @param ip 要绑定的IP地址，NULL表示绑定所有接口
 * @param port 端口号
 * @return socket fd，失败返回-1
 */
int create_tcp_socket(const char *ip, int port) {
    int fd;
    int opt = 1;
    struct sockaddr_in addr;

    /* 创建TCP socket (SOCK_STREAM面向字节流，可靠连接) */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERR("socket create failed: %s", strerror(errno));
        return -1;
    }

    /* SO_REUSEADDR选项:
     *   允许绑定到已经使用的地址
     *   开发调试时重启程序不用等待TIME_WAIT
     *   生产环境也是好习惯
     */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERR("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* 填充地址结构 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;           /* IPv4 */
    addr.sin_port = htons(port);         /* 端口号转网络字节序 */

    if (ip == NULL) {
        /* NULL表示绑定所有地址(INADDR_ANY) */
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        /* 将点分十进制IP转为二进制格式 */
        if (inet_pton(AF_INET, ip, &addr.sin_addr) < 0) {
            LOG_ERR("inet_pton failed for IP: %s", ip);
            close(fd);
            return -1;
        }
    }

    /* 绑定IP和端口 */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("bind to %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    /* 开始监听，backlog=128表示等待队列长度 */
    if (listen(fd, 128) < 0) {
        LOG_ERR("listen on %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    LOG("TCP socket created and listening on %s:%d (fd=%d)", ip ? ip : "0.0.0.0", port, fd);
    return fd;
}

/**
 * 创建并绑定UDP监听socket
 *
 * UDP是数据包模式(socket类型SOCK_DGRAM)，不需要listen()
 *
 * @param ip 要绑定的IP地址，NULL表示绑定所有接口
 * @param port 端口号
 * @return socket fd，失败返回-1
 */
int create_udp_socket(const char *ip, int port) {
    int fd;
    struct sockaddr_in addr;

    /* 创建UDP socket (SOCK_DGRAM数据包，无连接) */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERR("UDP socket create failed: %s", strerror(errno));
        return -1;
    }

    /* 填充地址结构 */
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

    /* 绑定端口 */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("bind UDP to %s:%d failed: %s", ip ? ip : "INADDR_ANY", port, strerror(errno));
        close(fd);
        return -1;
    }

    LOG("UDP socket created and bound on %s:%d (fd=%d)", ip ? ip : "0.0.0.0", port, fd);
    return fd;
}

/* ============================================================
 * 会话管理函数实现
 * ============================================================ */

/**
 * 添加新会话到链表头部
 *
 * 采用头插法，原因:
 *   - 时间复杂度O(1)，适合频繁添加
 *   - 新会话更可能活跃，靠近表头便于查找
 *
 * @param fd 客户端socket fd
 * @param protocol TCP或UDP
 * @param client_ip 客户端IP字符串
 * @param target_ip 目标IP字符串(可选)
 * @return 0成功, -1失败
 */
int add_session(int fd, protocol_type_t protocol, const char *client_ip, const char *target_ip) {
    /* 分配会话结构内存 */
    proxy_session_t *session = (proxy_session_t *)malloc(sizeof(proxy_session_t));
    if (session == NULL) {
        LOG_ERR("malloc failed for session");
        return -1;
    }

    /* 清零结构体，初始化所有字段 */
    memset(session, 0, sizeof(proxy_session_t));
    session->fd = fd;
    session->protocol = protocol;
    session->state = SESSION_STATE_ACTIVE;

    /* 安全复制IP字符串(避免缓冲区溢出) */
    if (client_ip) {
        strncpy(session->client_ip, client_ip, INET_ADDRSTRLEN - 1);
    }
    if (target_ip) {
        strncpy(session->target_ip, target_ip, INET_ADDRSTRLEN - 1);
    }

    /* 记录时间戳 */
    session->create_time = time(NULL);
    session->last_active = time(NULL);

    /* 链表操作需要加锁保护 */
    pthread_mutex_lock(&g_config.session_lock);

    /* 头插法: 新会话的next指向当前链表头 */
    session->next = g_config.sessions;
    g_config.sessions = session;
    g_config.session_count++;

    pthread_mutex_unlock(&g_config.session_lock);

    LOG("Session added: fd=%d, protocol=%s, client=%s, target=%s",
        fd, protocol == PROTOCOL_TCP ? "TCP" : "UDP",
        client_ip ? client_ip : "unknown",
        target_ip ? target_ip : "unknown");

    return 0;
}

/**
 * 从链表中移除会话
 *
 * 遍历链表找到对应fd的节点，移除并释放内存
 * 如果该会话有target_fd，也会一并关闭
 *
 * @param fd 要移除的socket fd
 * @return 0成功, -1未找到
 */
int remove_session(int fd) {
    pthread_mutex_lock(&g_config.session_lock);

    proxy_session_t *prev = NULL;
    proxy_session_t *curr = g_config.sessions;

    /* 遍历链表查找目标节点 */
    while (curr != NULL) {
        if (curr->fd == fd) {
            /* 找到目标，从链表中摘除 */
            if (prev == NULL) {
                /* 目标是表头，更新表头指针 */
                g_config.sessions = curr->next;
            } else {
                /* 目标在中间，跳过当前节点 */
                prev->next = curr->next;
            }

            /* 关闭目标socket(如果存在) */
            if (curr->target_fd > 0) {
                close(curr->target_fd);
            }

            /* 释放内存 */
            free(curr);
            g_config.session_count--;

            pthread_mutex_unlock(&g_config.session_lock);
            LOG("Session removed: fd=%d", fd);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&g_config.session_lock);
    return -1;
}

/**
 * 根据fd查找会话
 *
 * @param fd socket文件描述符
 * @return 找到返回会话指针，未找到返回NULL
 */
proxy_session_t *find_session_by_fd(int fd) {
    pthread_mutex_lock(&g_config.session_lock);

    proxy_session_t *curr = g_config.sessions;
    while (curr != NULL) {
        if (curr->fd == fd) {
            pthread_mutex_unlock(&g_config.session_lock);
            return curr;
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&g_config.session_lock);
    return NULL;
}

/**
 * 清理超时会话
 *
 * 定期清理长时间不活跃的会话，释放资源
 * 300秒(5分钟)是默认超时时间
 *
 * @param timeout_secs 超时秒数
 */
void cleanup_timeout_sessions(int timeout_secs) {
    time_t now = time(NULL);

    pthread_mutex_lock(&g_config.session_lock);

    proxy_session_t *prev = NULL;
    proxy_session_t *curr = g_config.sessions;

    /* 遍历所有会话 */
    while (curr != NULL) {
        /* 检查是否超时 */
        if (now - curr->last_active > timeout_secs) {
            proxy_session_t *to_free = curr;

            /* 从链表摘除 */
            if (prev == NULL) {
                g_config.sessions = curr->next;
                curr = g_config.sessions;
            } else {
                prev->next = curr->next;
                curr = curr->next;
            }

            LOG("Cleaning up timeout session: fd=%d, idle=%ld secs",
                to_free->fd, (long)(now - to_free->last_active));

            /* 关闭socket并释放内存 */
            if (to_free->target_fd > 0) {
                close(to_free->target_fd);
            }
            close(to_free->fd);
            free(to_free);
            g_config.session_count--;
        } else {
            /* 未超时，继续遍历 */
            prev = curr;
            curr = curr->next;
        }
    }

    pthread_mutex_unlock(&g_config.session_lock);
}

/* ============================================================
 * TCP长连接管理函数实现
 * ============================================================ */

/**
 * 查找或创建到目标IP的TCP长连接
 *
 * 这是TCP长连接复用的核心:
 *   - 多个请求可以复用同一个到目标服务器的连接
 *   - 避免频繁创建/销毁连接的开销
 *   - 提高通信效率
 *
 * 流程:
 *   1. 线程安全地加锁
 *   2. 在缓存中查找是否存在可用连接
 *   3. 存在则增加引用计数并返回
 *   4. 不存在则创建新TCP连接并加入缓存
 *   5. 解锁并返回
 *
 * @param target_ip 目标服务器IP
 * @return 连接条指针，失败返回NULL
 */
tcp_conn_entry_t *find_or_create_conn(const char *target_ip) {
    pthread_mutex_lock(&g_config.conn_lock);

    /* 第一步：在缓存中查找现有连接 */
    tcp_conn_entry_t *curr = g_config.tcp_conns;
    while (curr != NULL) {
        /* 匹配条件: IP相同 且 socket有效(fd >= 0) */
        if (strcmp(curr->target_ip, target_ip) == 0 && curr->target_fd >= 0) {
            /* 找到可用连接，增加引用计数 */
            curr->ref_count++;
            curr->last_active = time(NULL);
            pthread_mutex_unlock(&g_config.conn_lock);

            LOG("Found existing connection to %s (fd=%d, ref=%d)",
                target_ip, curr->target_fd, curr->ref_count);
            return curr;
        }
        curr = curr->next;
    }

    /* 第二步：缓存中没有，创建一个新连接 */

    /* 创建到目标IP的TCP socket */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        pthread_mutex_unlock(&g_config.conn_lock);
        LOG_ERR("Failed to create socket for %s: %s", target_ip, strerror(errno));
        return NULL;
    }

    /* 填充目标地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT_10);  /* 目标端口 */
    inet_pton(AF_INET, target_ip, &addr.sin_addr);

    /* 连接到目标服务器 */
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("Failed to connect to %s:%d: %s", target_ip, LISTEN_PORT_10, strerror(errno));
        close(fd);
        pthread_mutex_unlock(&g_config.conn_lock);
        return NULL;
    }

    /* 设置为非阻塞，便于epoll管理 */
    set_nonblocking(fd);

    /* 创建缓存条目 */
    tcp_conn_entry_t *entry = (tcp_conn_entry_t *)malloc(sizeof(tcp_conn_entry_t));
    if (entry == NULL) {
        close(fd);
        pthread_mutex_unlock(&g_config.conn_lock);
        return NULL;
    }

    /* 初始化缓存条目 */
    memset(entry, 0, sizeof(tcp_conn_entry_t));
    strncpy(entry->target_ip, target_ip, INET_ADDRSTRLEN - 1);
    entry->target_fd = fd;
    entry->last_active = time(NULL);
    entry->ref_count = 1;  /* 初始引用计数为1 */

    /* 头插法加入缓存链表 */
    entry->next = g_config.tcp_conns;
    g_config.tcp_conns = entry;

    pthread_mutex_unlock(&g_config.conn_lock);
    LOG("Created new connection to %s (fd=%d)", target_ip, fd);

    return entry;
}

/**
 * 释放TCP连接引用
 *
 * 当一个请求使用完连接后调用此函数
 * 减少引用计数，如果计数为0则关闭socket
 * (实际内存会在cleanup_dead_conns时释放)
 *
 * @param conn 连接条目指针
 */
void release_conn(tcp_conn_entry_t *conn) {
    if (conn == NULL) return;

    pthread_mutex_lock(&g_config.conn_lock);
    conn->ref_count--;              /* 引用计数减1 */
    conn->last_active = time(NULL);

    /* 引用计数为0且连接有效时，关闭socket */
    if (conn->ref_count <= 0 && conn->target_fd >= 0) {
        close(conn->target_fd);
        conn->target_fd = -1;  /* 标记为无效，供cleanup识别 */
        LOG("Closed idle connection to %s", conn->target_ip);
    }
    pthread_mutex_unlock(&g_config.conn_lock);
}

/**
 * 清理无效的TCP连接
 *
 * 移除两种无效条目:
 *   1. target_fd < 0 (已关闭但未释放内存)
 *   2. ref_count <= 0 (无引用且已关闭)
 *
 * 由主循环定期调用，防止内存泄漏
 */
void cleanup_dead_conns(void) {
    pthread_mutex_lock(&g_config.conn_lock);

    tcp_conn_entry_t *prev = NULL;
    tcp_conn_entry_t *curr = g_config.tcp_conns;

    while (curr != NULL) {
        /* 检查是否无效 */
        if (curr->target_fd < 0 || curr->ref_count <= 0) {
            tcp_conn_entry_t *to_free = curr;

            /* 从链表摘除 */
            if (prev == NULL) {
                g_config.tcp_conns = curr->next;
                curr = g_config.tcp_conns;
            } else {
                prev->next = curr->next;
                curr = curr->next;
            }

            /* 释放内存 */
            free(to_free);
        } else {
            /* 有效连接，移到下一个 */
            prev = curr;
            curr = curr->next;
        }
    }

    pthread_mutex_unlock(&g_config.conn_lock);
}

/* ============================================================
 * 初始化和事件处理函数实现
 * ============================================================ */

/**
 * 初始化所有监听socket
 *
 * 创建4个监听socket并加入epoll:
 *   - 192.168网段: TCP + UDP
 *   - 10.10.111网段: TCP + UDP
 *
 * @return 0成功, -1失败
 */
int init_listen_sockets(void) {
    LOG("Initializing listening sockets...");

    /* 初始化互斥锁 */
    pthread_mutex_init(&g_config.session_lock, NULL);
    pthread_mutex_init(&g_config.conn_lock, NULL);

    /* 创建epoll实例，Linux 2.6+推荐使用epoll_create1 */
    g_config.epoll_fd = epoll_create1(0);
    if (g_config.epoll_fd < 0) {
        LOG_ERR("epoll_create1 failed: %s", strerror(errno));
        return -1;
    }
    LOG("Epoll created (fd=%d)", g_config.epoll_fd);

    /* 创建192.168网段的TCP和UDP监听socket */
    g_config.tcp_listen_fd_192 = create_tcp_socket("192.168.3.10", LISTEN_PORT_192);
    if (g_config.tcp_listen_fd_192 < 0) {
        LOG_ERR("Failed to create TCP socket on 192.168");
        return -1;
    }

    g_config.udp_listen_fd_192 = create_udp_socket("192.168.3.10", LISTEN_PORT_192);
    if (g_config.udp_listen_fd_192 < 0) {
        LOG_ERR("Failed to create UDP socket on 192.168");
        return -1;
    }

    /* 创建10.10.111网段的TCP和UDP监听socket */
    g_config.tcp_listen_fd_10 = create_tcp_socket("10.10.111.10", LISTEN_PORT_10);
    if (g_config.tcp_listen_fd_10 < 0) {
        LOG_ERR("Failed to create TCP socket on 10.10.111");
        return -1;
    }

    g_config.udp_listen_fd_10 = create_udp_socket("10.10.111.10", LISTEN_PORT_10);
    if (g_config.udp_listen_fd_10 < 0) {
        LOG_ERR("Failed to create UDP socket on 10.10.111");
        return -1;
    }

    /* 将4个监听socket加入epoll统一管理 */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  /* 监听可读事件，边缘触发 */

    /* 192.168 TCP */
    ev.data.fd = g_config.tcp_listen_fd_192;
    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.tcp_listen_fd_192, &ev) < 0) {
        LOG_ERR("epoll_ctl ADD tcp_listen_fd_192 failed: %s", strerror(errno));
        return -1;
    }

    /* 192.168 UDP */
    ev.data.fd = g_config.udp_listen_fd_192;
    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.udp_listen_fd_192, &ev) < 0) {
        LOG_ERR("epoll_ctl ADD udp_listen_fd_192 failed: %s", strerror(errno));
        return -1;
    }

    /* 10.10.111 TCP */
    ev.data.fd = g_config.tcp_listen_fd_10;
    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.tcp_listen_fd_10, &ev) < 0) {
        LOG_ERR("epoll_ctl ADD tcp_listen_fd_10 failed: %s", strerror(errno));
        return -1;
    }

    /* 10.10.111 UDP */
    ev.data.fd = g_config.udp_listen_fd_10;
    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, g_config.udp_listen_fd_10, &ev) < 0) {
        LOG_ERR("epoll_ctl ADD udp_listen_fd_10 failed: %s", strerror(errno));
        return -1;
    }

    LOG("All listening sockets added to epoll");
    return 0;
}

/**
 * 处理TCP数据转发
 *
 * 这是TCP转发的核心函数，处理流程:
 *   1. 从数据前16字节解析目标IP
 *   2. 验证目标IP属于10.10.111网段
 *   3. 获取或创建到目标的长连接
 *   4. 发送数据(跳过16字节IP头)
 *   5. 接收响应
 *   6. 将响应返回给客户端
 *   7. 释放连接(长连接保留供复用)
 *
 * @param src_fd 源socket (客户端连接)
 * @param target_ip 目标IP(可选，为NULL时从数据中解析)
 * @param buffer 接收到的数据
 * @param len 数据长度
 * @return 0成功, -1失败
 */
int forward_tcp_to_target(int src_fd, const char *target_ip, char *buffer, int len) {
    /* 长度检查：至少需要16字节存储目标IP */
    if (len < 16) {
        LOG("TCP data too short: %d bytes, need at least 16 for target IP", len);
        return -1;
    }

    /* 从数据前16字节提取目标IP字符串 */
    char extracted_ip[16];
    memcpy(extracted_ip, buffer, 15);
    extracted_ip[15] = '\0';

    /* 确定最终目标IP: 优先使用传入参数，否则用解析出的IP */
    const char *final_target_ip = target_ip ? target_ip : extracted_ip;
    if (target_ip && strcmp(extracted_ip, target_ip) != 0) {
        /* 如果两者不一致，记录警告但使用提供的IP */
        LOG("Warning: extracted IP (%s) != provided target IP (%s), using provided",
            extracted_ip, target_ip);
    }

    /* 检查目标IP是否10.10.111网段，拒绝非法目标 */
    if (!is_target_network(final_target_ip)) {
        LOG("Target IP %s is not in 10.10.111.x network, dropping", final_target_ip);
        return -1;
    }

    LOG("TCP forwarding: %d bytes to %s (extracted from data)", len - 16, final_target_ip);

    /* 获取到目标的TCP长连接 */
    tcp_conn_entry_t *conn = find_or_create_conn(final_target_ip);
    if (conn == NULL) {
        LOG_ERR("Failed to get connection to %s", final_target_ip);
        return -1;
    }

    /* 计算实际数据长度和位置(跳过前16字节IP) */
    int payload_len = len - 16;
    char *payload = buffer + 16;

    /* 通过长连接发送数据到目标 */
    int sent = send(conn->target_fd, payload, payload_len, 0);
    if (sent < 0) {
        LOG_ERR("send to %s failed: %s", final_target_ip, strerror(errno));
        release_conn(conn);
        return -1;
    }

    LOG("Sent %d bytes to %s, waiting for response...", sent, final_target_ip);

    /* 接收目标返回的响应 */
    char response[BUFFER_SIZE];
    int resp_len = recv(conn->target_fd, response, sizeof(response), 0);
    if (resp_len > 0) {
        LOG("Received %d bytes response from %s", resp_len, final_target_ip);
        /* 将响应返回给客户端 */
        int sent_back = send(src_fd, response, resp_len, 0);
        LOG("Sent %d bytes response back to client (fd=%d)", sent_back, src_fd);
    } else if (resp_len == 0) {
        /* 目标关闭了连接 */
        LOG("Connection closed by %s", final_target_ip);
    } else {
        LOG_ERR("recv from %s failed: %s", final_target_ip, strerror(errno));
    }

    /* 释放连接引用，长连接保留供下次使用 */
    release_conn(conn);

    return resp_len > 0 ? 0 : -1;
}

/**
 * 处理UDP数据转发
 *
 * UDP转发流程:
 *   1. 从数据前16字节解析目标IP
 *   2. 验证目标IP属于10.10.111网段
 *   3. 创建临时UDP socket
 *   4. 发送数据(跳过16字节IP头)
 *   5. 关闭临时socket
 *
 * 注意: UDP是无连接的，不需要响应，发送完即可
 *
 * @param target_ip 目标IP(可选，为NULL时从数据中解析)
 * @param buffer 接收到的数据
 * @param len 数据长度
 * @return 0成功, -1失败
 */
int forward_udp_to_target(const char *target_ip, char *buffer, int len) {
    /* 长度检查 */
    if (len < 16) {
        LOG("UDP data too short: %d bytes, need at least 16 for target IP", len);
        return -1;
    }

    /* 提取前16字节的目标IP */
    char extracted_ip[16];
    memcpy(extracted_ip, buffer, 15);
    extracted_ip[15] = '\0';

    const char *final_target_ip = target_ip ? target_ip : extracted_ip;

    /* 验证目标IP */
    if (!is_target_network(final_target_ip)) {
        LOG("Target IP %s is not in 10.10.111.x network, dropping", final_target_ip);
        return -1;
    }

    LOG("UDP forwarding: %d bytes to %s (no response expected)", len - 16, final_target_ip);

    /* 创建临时UDP socket用于发送 */
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERR("Failed to create UDP socket for forward: %s", strerror(errno));
        return -1;
    }

    /* 填充目标地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT_10);
    inet_pton(AF_INET, final_target_ip, &addr.sin_addr);

    /* 计算实际数据长度和位置 */
    int payload_len = len - 16;
    char *payload = buffer + 16;

    /* 发送数据到目标 */
    int sent = sendto(fd, payload, payload_len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        LOG_ERR("sendto %s failed: %s", final_target_ip, strerror(errno));
        close(fd);
        return -1;
    }

    LOG("UDP forwarded %d bytes to %s", sent, final_target_ip);
    close(fd);  /* 临时socket用完即关 */

    return 0;
}

/**
 * 处理TCP客户端数据
 *
 * 从客户端socket读取数据并转发
 *
 * @param fd 客户端socket fd
 */
void handle_tcp_data(int fd) {
    char buffer[BUFFER_SIZE];

    /* 读取客户端发送的数据 */
    int n = read(fd, buffer, sizeof(buffer));

    if (n > 0) {
        /* 成功读取到数据 */
        buffer[n] = '\0';
        LOG("TCP read %d bytes from fd=%d", n, fd);

        /* 更新会话活跃时间，防止被清理掉 */
        proxy_session_t *session = find_session_by_fd(fd);
        if (session) {
            session->last_active = time(NULL);
        }

        /* 转发数据到目标，并等待响应返回给客户端 */
        forward_tcp_to_target(fd, NULL, buffer, n);

    } else if (n == 0) {
        /* 客户端关闭了连接(收到FIN) */
        LOG("TCP connection closed (fd=%d)", fd);
        close(fd);
        remove_session(fd);

    } else {
        /* 读取出错(非阻塞模式下EAGAIN是正常的) */
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERR("read from fd=%d failed: %s", fd, strerror(errno));
            close(fd);
            remove_session(fd);
        }
    }
}

/**
 * 处理UDP数据
 *
 * 从UDP socket接收数据并转发到目标
 *
 * @param fd UDP监听socket
 * @param client_addr 客户端地址信息
 * @param addr_len 地址结构长度
 */
void handle_udp_data(int fd, struct sockaddr_in *client_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    /* 接收UDP数据报 */
    int recv_len = recvfrom(fd, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)client_addr, &addr_len);

    if (recv_len < 0) {
        /* 接收出错(非阻塞模式下EAGAIN是正常的) */
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERR("recvfrom fd=%d failed: %s", fd, strerror(errno));
        }
        return;
    }

    /* 记录客户端信息用于日志 */
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    LOG("UDP received %d bytes from %s:%d (fd=%d)",
        recv_len, client_ip, ntohs(client_addr->sin_port), fd);

    /* 转发数据到目标，UDP不需要响应 */
    forward_udp_to_target(NULL, buffer, recv_len);
}

/**
 * 处理TCP监听socket的新连接
 *
 * 当epoll通知TCP监听socket可读时，说明有新的客户端连接
 * 调用accept接受连接，加入epoll，添加到会话链表
 *
 * @param listen_fd TCP监听socket
 */
void handle_tcp_accept(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];

    /* 接受新连接 */
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        LOG_ERR("accept failed on fd=%d: %s", listen_fd, strerror(errno));
        return;
    }

    /* 记录客户端IP用于日志 */
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    LOG("TCP new connection from %s:%d (fd=%d)", client_ip, ntohs(client_addr.sin_port), client_fd);

    /* 设置为非阻塞，方便后续epoll事件处理 */
    set_nonblocking(client_fd);

    /* 添加到epoll，监听其可读事件 */
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  /* 边沿触发可读事件 */
    ev.data.fd = client_fd;

    if (epoll_ctl(g_config.epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
        LOG_ERR("epoll_ctl ADD client_fd=%d failed: %s", client_fd, strerror(errno));
        close(client_fd);
        return;
    }

    /* 添加到会话链表跟踪 */
    add_session(client_fd, PROTOCOL_TCP, client_ip, NULL);
}

/**
 * 主事件循环
 *
 * 程序的核心: 使用epoll_wait等待I/O事件并处理
 *
 * 事件类型:
 *   - TCP监听socket可读 -> 调用handle_tcp_accept
 *   - UDP监听socket可读 -> 调用handle_udp_data
 *   - 普通TCP socket可读 -> 调用handle_tcp_data
 *   - 错误/断开事件 -> 清理会话
 *
 * 超时清理:
 *   每60秒调用cleanup清理超时会话和无效连接
 *
 * 阻塞行为:
 *   epoll_wait阻塞直到有事件发生或超时
 *   timeout=1000ms，兼顾响应性和低CPU占用
 */
void run_event_loop(void) {
    struct epoll_event events[MAX_EVENTS];

    LOG("Event loop started, waiting for events...");

    /* 主循环 */
    while (1) {
        /* 等待事件，timeout=1秒 */
        int nfds = epoll_wait(g_config.epoll_fd, events, MAX_EVENTS, 1000);

        if (nfds < 0) {
            /* 信号中断继续等待 */
            if (errno == EINTR) continue;
            LOG_ERR("epoll_wait failed: %s", strerror(errno));
            break;
        }

        /* 处理所有就绪的事件 */
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;

            /* 判断是哪种socket的事件 */

            /* TCP监听socket -> 有新连接 */
            if (fd == g_config.tcp_listen_fd_192 || fd == g_config.tcp_listen_fd_10) {
                if (event_mask & EPOLLIN) {
                    handle_tcp_accept(fd);
                }
            }
            /* UDP监听socket -> 有数据到达 */
            else if (fd == g_config.udp_listen_fd_192 || fd == g_config.udp_listen_fd_10) {
                if (event_mask & EPOLLIN) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    handle_udp_data(fd, &client_addr, addr_len);
                }
            }
            /* 错误/断开事件 */
            else if (event_mask & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                LOG("Connection error/hup on fd=%d", fd);
                close(fd);
                remove_session(fd);
            }
            /* 普通TCP socket可读 */
            else if (event_mask & EPOLLIN) {
                handle_tcp_data(fd);
            }
        }

        /* 定期清理：每60秒执行一次 */
        static time_t last_cleanup = 0;
        time_t now = time(NULL);
        if (now - last_cleanup > 60) {
            cleanup_timeout_sessions(300);  /* 清理5分钟未活跃的会话 */
            cleanup_dead_conns();            /* 清理无效TCP连接 */
            last_cleanup = now;
            LOG("Session cleanup done, current sessions: %d", g_config.session_count);
        }
    }
}

/* ============================================================
 * 主函数和信号处理
 * ============================================================ */

/**
 * 信号处理函数
 *
 * 捕获SIGINT(Ctrl+C)和SIGTERM(kill)信号
 * 优雅退出程序
 *
 * @param sig 信号编号
 */
static void signal_handler(int sig) {
    (void)sig;  /* 未使用参数，避免警告 */
    LOG("Received signal %d, shutting down...", sig);
    exit(0);
}

/**
 * 程序入口点
 *
 * 初始化阶段:
 *   1. 打印欢迎信息
 *   2. 注册信号处理
 *   3. 初始化监听socket
 *
 * 运行阶段:
 *   4. 进入事件循环处理数据转发
 *
 * 退出阶段:
 *   5. 关闭所有socket
 *   6. 销毁互斥锁
 *   7. 返回0
 */
int main(int argc, char *argv[]) {
    /* 未使用参数，避免警告 */
    (void)argc;
    (void)argv;

    /* 打印程序信息 */
    printf("=========================================\n");
    printf("    Net Proxy RPC Server\n");
    printf("=========================================\n");
    printf("Listening on:\n");
    printf("  - 192.168.3.10:%d (TCP+UDP)\n", LISTEN_PORT_192);
    printf("  - 10.10.111.10:%d (TCP+UDP)\n", LISTEN_PORT_10);
    printf("Target network: %s.x\n", TARGET_NET);
    printf("=========================================\n\n");

    /* 注册信号处理，实现优雅退出 */
    signal(SIGINT, signal_handler);   /* Ctrl+C */
    signal(SIGTERM, signal_handler);   /* kill */

    /* 初始化4个监听socket */
    if (init_listen_sockets() < 0) {
        LOG_ERR("Failed to initialize listening sockets");
        return 1;
    }

    /* 进入事件循环，永不返回(直到收到信号) */
    run_event_loop();

    /* 以下代码理论上不会执行，因为run_event_loop被信号中断会调用exit() */

    /* 清理：关闭所有socket */
    if (g_config.tcp_listen_fd_192 >= 0) close(g_config.tcp_listen_fd_192);
    if (g_config.tcp_listen_fd_10 >= 0) close(g_config.tcp_listen_fd_10);
    if (g_config.udp_listen_fd_192 >= 0) close(g_config.udp_listen_fd_192);
    if (g_config.udp_listen_fd_10 >= 0) close(g_config.udp_listen_fd_10);
    if (g_config.epoll_fd >= 0) close(g_config.epoll_fd);

    /* 销毁互斥锁 */
    pthread_mutex_destroy(&g_config.session_lock);
    pthread_mutex_destroy(&g_config.conn_lock);

    return 0;
}