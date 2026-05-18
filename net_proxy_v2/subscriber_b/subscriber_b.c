/**
 * ============================================================
 * Subscriber B - VSOA数据订阅客户端
 * ============================================================
 *
 * 功能说明:
 *   - 部署在10.10.111网段的设备上，与程序2部署在一起
 *   - 通过VSOA DDS订阅来自程序2发布的数据
 *   - 对接收到的数据进行打印，用于验证数据转发是否成功
 *
 * VSOA订阅流程:
 *   1. 初始化订阅者
 *   2. 订阅指定主题
 *   3. 等待并处理接收到的数据
 *   4. 打印数据内容
 *
 * 使用方法:
 *   ./subscriber_b [主题名]
 *   例如: ./subscriber_b proxy_data_topic
 *
 * 作者: Claude Code
 * 日期: 2026-04-28
 */

#include "../common/common.h"
#include <pthread.h>

/* 订阅者配置 */
#define DEFAULT_TOPIC "proxy_data_topic"  /* 默认订阅主题 */
#define VSOA_SERVICE_NAME "subscriber_b"   /* 本服务名称 */

/**
 * VSOA订阅者初始化 (模拟实现)
 * 实际使用时需要调用真实的VSOA订阅者API
 */
void *vsoa_subscriber_init(const char *service_name) {
    static int mock_sub_id = 1;
    int *subscriber = (int *)malloc(sizeof(int));
    if (subscriber != NULL) {
        *subscriber = mock_sub_id++;
        LOG("VSOA Subscriber initialized: %s (id=%d)", service_name ? service_name : "unnamed", *subscriber);
    }
    return subscriber;
}

/**
 * 订阅主题 (模拟实现)
 */
int vsoa_subscribe(void *subscriber, const char *topic) {
    if (subscriber == NULL || topic == NULL) {
        LOG_ERR("Invalid subscriber or topic");
        return -1;
    }

    int sub_id = *(int *)subscriber;
    LOG("Subscribed to topic: %s (subscriber id=%d)", topic, sub_id);

    /* 实际应该调用 vsoa_subscribe(subscriber, topic) */

    return 0;
}

/**
 * 接收数据 (模拟实现)
 * 实际使用时这是一个回调函数或阻塞调用
 */
int vsoa_receive(void *subscriber, char *buffer, int buffer_size, int timeout_ms) {
    if (subscriber == NULL || buffer == NULL) {
        return -1;
    }

    /* 模拟: 等待一段时间后返回模拟数据 */
    usleep(timeout_ms * 1000);

    /* 这里模拟接收到了一个数据 */
    /* 实际实现中，数据通过回调函数或消息队列传递 */

    return 0;  /* 返回0表示没有数据可用 */
}

/**
 * 关闭订阅者
 */
void vsoa_subscriber_close(void *subscriber) {
    if (subscriber != NULL) {
        int sub_id = *(int *)subscriber;
        LOG("VSOA Subscriber closed: id=%d", sub_id);
        free(subscriber);
    }
}

/**
 * 打印数据内容
 */
void print_data(const char *data, int len) {
    if (data == NULL || len <= 0) {
        return;
    }

    printf("=========================================\n");
    printf("    Received Data\n");
    printf("=========================================\n");
    printf("Length: %d bytes\n", len);
    printf("Content:\n");

    /* 打印十六进制和ASCII字符 */
    printf("Hex: ");
    for (int i = 0; i < len && i < 64; i++) {
        printf("%02x ", (unsigned char)data[i]);
        if ((i + 1) % 16 == 0) printf("\n     ");
    }
    printf("\n");

    /* 打印可读字符 */
    printf("ASCII: ");
    for (int i = 0; i < len && i < 64; i++) {
        unsigned char c = (unsigned char)data[i];
        printf("%c", (c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");

    /* 如果是字符串，单独打印字符串内容 */
    if (len < 256) {
        printf("String: %.*s\n", len, data);
    }

    printf("=========================================\n\n");
}

/**
 * 数据处理线程
 */
void *data_process_thread(void *arg) {
    (void)arg;

    LOG("Data processing thread started");

    /* 模拟接收到的数据 */
    static int data_count = 0;

    while (1) {
        /* 模拟: 每秒检查一次新数据 */
        sleep(1);

        /* 这里应该是实际从VSOA接收数据的逻辑 */
        /* 由于是模拟实现，我们只打印日志 */

        /* 模拟数据到达 */
        data_count++;
        if (data_count % 5 == 0) {
            /* 模拟每5秒收到一条数据 */
            char mock_data[256];
            snprintf(mock_data, sizeof(mock_data),
                     "Simulated data #%d from proxy server at %ld",
                     data_count,
                     (long)time(NULL));

            LOG("Simulated VSOA data received: %s", mock_data);
            print_data(mock_data, strlen(mock_data));
        }
    }

    return NULL;
}

/**
 * 信号处理
 */
static void signal_handler(int sig) {
    (void)sig;
    LOG("Received signal, shutting down subscriber...");
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *topic = DEFAULT_TOPIC;

    /* 解析命令行参数 */
    if (argc >= 2) {
        topic = argv[1];
    }

    printf("=========================================\n");
    printf("    Subscriber B - VSOA Data Receiver\n");
    printf("=========================================\n");
    printf("Subscribing to topic: %s\n", topic);
    printf("=========================================\n\n");

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 初始化VSOA订阅者 */
    void *subscriber = vsoa_subscriber_init(VSOA_SERVICE_NAME);
    if (subscriber == NULL) {
        LOG_ERR("Failed to initialize VSOA subscriber");
        return 1;
    }

    /* 订阅主题 */
    if (vsoa_subscribe(subscriber, topic) < 0) {
        LOG_ERR("Failed to subscribe to topic: %s", topic);
        vsoa_subscriber_close(subscriber);
        return 1;
    }

    LOG("Successfully subscribed to topic: %s", topic);
    LOG("Waiting for data... (Press Ctrl+C to exit)");

    /* 启动数据处理线程 */
    pthread_t tid;
    if (pthread_create(&tid, NULL, data_process_thread, NULL) != 0) {
        LOG_ERR("Failed to create data processing thread");
        vsoa_subscriber_close(subscriber);
        return 1;
    }

    /* 主线程等待信号 */
    while (1) {
        sleep(1);
    }

    /* 永远不会执行到这里，因为信号处理会退出程序 */
    pthread_join(tid, NULL);
    vsoa_subscriber_close(subscriber);

    return 0;
}