/**
 * ============================================================
 * Client A - 测试客户端程序
 * ============================================================
 *
 * 功能说明:
 *   - 部署在192.168网段的设备上，与程序1部署在一起
 *   - 不断向程序1发送不同类型的数据
 *   - 支持TCP和UDP两种协议
 *   - 使用自定义协议格式封装数据
 *
 * 数据格式:
 *   - 协议头(8字节): [魔数2字节][版本1字节][类型1字节][保留4字节]
 *   - 消息头(32字节): [序列号4字节][时间戳8字节][源IP16字节]
 *   - 负载数据: [16字节目标IP][实际数据]
 *
 * 使用方法:
 *   ./client_a [目标IP] [端口] [协议]
 *   例如: ./client_a 192.168.3.10 8888 tcp
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#include "../common/common.h"
#include <sys/time.h>

/* 客户端配置 */
#define DEFAULT_TARGET_IP "192.168.3.10"  /* 程序1的IP */
#define DEFAULT_TARGET_PORT 8888          /* 程序1的端口 */
#define DEFAULT_PROTOCOL "tcp"           /* 默认协议 */
#define SEND_INTERVAL_MS 1000            /* 发送间隔(毫秒) */

/* 最大数据大小 */
#define MAX_DATA_SIZE 1024

/**
 * 获取当前时间戳(毫秒)
 */
static uint64_t get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * 序列号生成器
 */
static uint32_t g_seq = 0;
static uint32_t get_next_seq(void) {
    return ++g_seq;
}

/**
 * 发送TCP数据
 */
int send_tcp_data(const char *target_ip, int port, const char *data, int len) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERR("Failed to create TCP socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERR("Failed to connect to %s:%d: %s", target_ip, port, strerror(errno));
        close(fd);
        return -1;
    }

    /* 发送数据 */
    int sent = send(fd, data, len, 0);
    if (sent < 0) {
        LOG_ERR("Failed to send data: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOG("TCP sent %d bytes", sent);

    /* 接收响应(如果需要) */
    char response[MAX_BUFFER_SIZE];
    int resp_len = recv(fd, response, sizeof(response), 0);
    if (resp_len > 0) {
        LOG("TCP received response: %d bytes", resp_len);
    }

    close(fd);
    return 0;
}

/**
 * 发送UDP数据
 */
int send_udp_data(const char *target_ip, int port, const char *data, int len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_ERR("Failed to create UDP socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &addr.sin_addr);

    /* 发送数据 */
    int sent = sendto(fd, data, len, 0, (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        LOG_ERR("Failed to send UDP data: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOG("UDP sent %d bytes to %s:%d", sent, target_ip, port);
    close(fd);
    return 0;
}

/**
 * 构建测试数据
 * 数据格式: [16字节目标IP][实际数据]
 */
int build_test_data(char *buffer, int buffer_size, const char *dest_ip, const char *msg_data, int msg_len) {
    if (buffer_size < 16 + msg_len) {
        LOG_ERR("Buffer too small");
        return -1;
    }

    /* 前16字节目标IP */
    memset(buffer, 0, 16);
    strncpy(buffer, dest_ip, 15);

    /* 后续是实际数据 */
    memcpy(buffer + 16, msg_data, msg_len);

    return 16 + msg_len;
}

/**
 * 生成测试消息
 */
char *generate_test_message(int msg_type, int *out_len) {
    static char msg[256];
    static int msg_count = 0;

    /* 生成不同的测试数据 */
    snprintf(msg, sizeof(msg),
             "Test message #%d from Client_A at %llu ms, type=%d",
             ++msg_count,
             (unsigned long long)get_time_ms(),
             msg_type);

    *out_len = strlen(msg);
    return msg;
}

int main(int argc, char *argv[]) {
    const char *target_ip = DEFAULT_TARGET_IP;
    int target_port = DEFAULT_TARGET_PORT;
    const char *protocol = DEFAULT_PROTOCOL;

    /* 解析命令行参数 */
    if (argc >= 2) target_ip = argv[1];
    if (argc >= 3) target_port = atoi(argv[2]);
    if (argc >= 4) protocol = argv[3];

    printf("=========================================\n");
    printf("    Client A - Test Data Sender\n");
    printf("=========================================\n");
    printf("Target: %s:%d\n", target_ip, target_port);
    printf("Protocol: %s\n", protocol);
    printf("=========================================\n\n");

    /* 创建协议头 */
    protocol_header_t header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.reserved = 0;

    /* 消息类型数组，用于循环发送不同类型 */
    uint8_t msg_types[] = {MSG_TYPE_TEXT, MSG_TYPE_TEXT, MSG_TYPE_BINARY, MSG_TYPE_HEARTBEAT};
    int msg_type_count = sizeof(msg_types) / sizeof(msg_types[0]);
    int msg_idx = 0;

    LOG("Starting to send data every %d ms...", SEND_INTERVAL_MS);

    while (1) {
        /* 获取下一个消息类型 */
        uint8_t current_type = msg_types[msg_idx % msg_type_count];
        msg_idx++;

        /* 生成测试数据 */
        int data_len = 0;
        char *msg_data = generate_test_message(current_type, &data_len);

        /* 构建完整数据包: [协议头][消息头][目标IP(16字节)][数据] */
        /* 先构建一个用于发送的完整buffer */
        char send_buffer[MAX_BUFFER_SIZE];

        /* 复制协议头 */
        memcpy(send_buffer, &header, sizeof(protocol_header_t));

        /* 复制消息头 */
        message_header_t msg_header;
        msg_header.seq_num = get_next_seq();
        msg_header.timestamp = get_time_ms();
        strncpy(msg_header.src_ip, "192.168.3.100", 15);
        msg_header.src_ip[15] = '\0';
        memcpy(send_buffer + sizeof(protocol_header_t), &msg_header, sizeof(message_header_t));

        /* 复制目标IP和数据 */
        int payload_len = build_test_data(
            send_buffer + sizeof(protocol_header_t) + sizeof(message_header_t),
            MAX_BUFFER_SIZE - sizeof(protocol_header_t) - sizeof(message_header_t),
            "10.10.111.20",  /* 假设目标是10.10.111.20 */
            msg_data,
            data_len
        );

        int total_len = sizeof(protocol_header_t) + sizeof(message_header_t) + payload_len;

        LOG("Sending message: seq=%u, type=%d, total_len=%d", msg_header.seq_num, current_type, total_len);

        /* 根据协议类型发送 */
        if (strcmp(protocol, "tcp") == 0) {
            send_tcp_data(target_ip, target_port, send_buffer, total_len);
        } else {
            send_udp_data(target_ip, target_port, send_buffer, total_len);
        }

        /* 等待一段时间 */
        usleep(SEND_INTERVAL_MS * 1000);
    }

    return 0;
}