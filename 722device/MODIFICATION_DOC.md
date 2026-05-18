# 722device 功能修改文档

## 修改日期
2026-05-05

## 修改版本
v3.0

## 背景

根据最新需求，对 722device 工程进行进一步优化：
1. 配置文件改为 JSON 格式解析（使用 cJSON）
2. TCP/UDP 发送数据简化为只含 url (topic) 和 payload
3. TCP/UDP 接收线程收到数据后，直接调用 `app_vsoa_publish_data()` 发布，无需队列

## 修改内容

### 1. network/tls_net_proxy.h

#### 1.1 新增 app_vsoa_publish_t 前向声明

```c
// Forward declaration
typedef struct _app_vsoa_publish app_vsoa_publish_t;
```

#### 1.2 修改 net_proxy_t 结构体

新增 `app_publish_` 字段用于直接 VSOA 发布，移除接收队列：

```c
typedef struct {
    S32                 tcp_fd;
    S32                 udp_fd;
    link_map_config_t   config;
    pthread_t           tcp_listen_thread_;
    pthread_t           udp_listen_thread_;
    pthread_t           transmit_thread_;         // 优先级队列发送线程
    bool                stop_listening_;
    void*               tls_server_;             // TLS server
    app_vsoa_publish_t* app_publish_;           // VSOA publish 句柄 (新增)
    net_proxy_prio_queue_t  prio_queues[MAX_PRIORITY_LEVELS];  // 优先级队列
} net_proxy_t;
```

#### 1.3 新增函数

```c
// 设置 VSOA publish 句柄
extern void tls_net_proxy_set_publish(app_vsoa_publish_t* publish);

// 使用 cJSON 加载 JSON 格式配置
extern S32 tls_net_proxy_load_config_json(const char* config_path, link_map_config_t* config);
```

---

### 2. network/tls_net_proxy.c

#### 2.1 JSON 配置解析 (cJSON)

使用 cJSON 库解析 JSON 格式配置文件：

```c
S32 tls_net_proxy_load_config_json(const char* config_path, link_map_config_t* config)
{
    cJSON *root = cJSON_Parse(buffer);

    // 解析 count, default_port, listen_tcp_port, listen_udp_port
    // 解析 entries 数组: id, dest_ip, dest_port
    // protocol 字段已废弃，不解析
}
```

#### 2.2 TCP/UDP 接收线程 - 直接 VSOA 发布

TCP/UDP 监听线程收到数据后，直接调用 `app_vsoa_publish_data()` 发布，无需队列：

Buffer 格式：`url_len(4字节网络序) + url + payload_data`

```c
// TCP 监听线程
static void* tls_net_proxy_tcp_listen_thread(void* arg)
{
    // ... 接收数据 ...
    ret = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (ret > 0)
    {
        // 从 buffer 中解析 URL 和 payload
        char url[256];
        U8* payload = NULL;
        U32 payload_len = 0;

        if (parse_url_payload_from_buffer(buffer, ret, url, sizeof(url), &payload, &payload_len) == 0)
        {
            vsoa_url.url = url;
            vsoa_payload.data = payload;
            vsoa_payload.data_len = payload_len;
        }
        else
        {
            // Fallback: 使用 /data 作为默认 topic
            vsoa_url.url = "/data";
            vsoa_payload.data = buffer;
            vsoa_payload.data_len = ret;
        }

        // 直接发布，无需队列
        if (proxy->app_publish_)
        {
            app_vsoa_publish_data(proxy->app_publish_, &vsoa_url, &vsoa_payload);
        }
    }
}

// UDP 监听线程同理
```

#### 2.3 发送线程 - 从优先级队列取数据发送

发送线程从优先级队列取数据，通过 TCP/UDP 发送。发送数据格式与接收一致：`url_len(4字节网络序) + url + payload_data`

```c
static void* tls_net_proxy_transmit_thread(void* arg)
{
    while (!proxy->stop_listening_)
    {
        // 从高优先级队列开始检查
        for (int i = 0; i < MAX_PRIORITY_LEVELS; i++)
        {
            // pop 数据
        }

        // TCP 或 UDP 发送（数据已封装为二进制格式）
        if (node.transmit_data.proto == PROTO_TCP)
            tls_net_proxy_send_tcp(dest_ip, dest_port, url, url_len, data, data_len);
        else if (node.transmit_data.proto == PROTO_UDP)
            tls_net_proxy_send_udp(dest_ip, dest_port, url, url_len, data, data_len);
    }
}
}
```

---

### 3. app/app_transmit.c

#### 3.1 修改 api_transmit_callback_on_app_rpc_server_message()

使用新的 JSON 配置加载函数和 publish 设置：

```c
if (s_link_map_config.count == 0)
{
    api_transmit_get_app_path(app_transmit->app_path_, sizeof(app_transmit->app_path_));
    strcpy(config_path, app_transmit->app_path_);
    strcat(config_path, "tlsconfig/link_map.json");
    if (tls_net_proxy_load_config_json(config_path, &s_link_map_config) == 0)
    {
        tls_net_proxy_init(&s_link_map_config);
        tls_net_proxy_set_server(app_transmit->tls_server_);
        tls_net_proxy_set_publish(app_transmit->app_publish);  // 新增
        tls_net_proxy_start_listening(&g_net_proxy);
    }
}
```

---

## 数据流图

### VSOA RPC 发送流程

```
[WSL Client] VSOA RPC
    URL: /toLink/89/1/topic/device
    Payload: {"temp": 25.5}

    ↓

[722device] RPC Server 接收
    ↓

[app_transmit.c] api_transmit_callback_on_app_rpc_server_message()
    - 解析 URL: id=89, proto=1, topic=/topic/device
    - 查找 link_map[89] → ip="192.168.1.100", port=8888
    - 推送到优先级队列

    ↓

[tls_net_proxy.c] 发送线程
    - 从优先级队列取数据
    - 通过 UDP 发送到 192.168.1.100:8888
```

### TCP/UDP 接收发布流程

```
[外部设备] 发送 TCP/UDP 数据到 722device

    ↓

[tls_net_proxy.c] TCP/UDP 监听线程
    - recv() / recvfrom() 接收数据
    - 直接调用 app_vsoa_publish_data()
    - 发布到 VSOA (topic: /data)
```

---

## link_map.json 配置示例

**使用 JSON 格式，protocol 字段已废弃：**

```json
{
    "count": 3,
    "default_port": 9000,
    "listen_tcp_port": 9000,
    "listen_udp_port": 9001,
    "entries": [
        {"id": 89, "dest_ip": "192.168.1.100", "dest_port": 8888},
        {"id": 90, "dest_ip": "192.168.1.101", "dest_port": 8889},
        {"id": 91, "dest_ip": "192.168.1.102", "dest_port": 8890}
    ]
}
```

---

## 关键 URL 格式

| 来源 | URL 格式 | 示例 |
|------|----------|------|
| VSOA RPC 发送 | /toLink/<id>/<proto>/<topic>/... | /toLink/89/1/topic/data |
| TCP 接收发布 | /data (默认) | /data |
| UDP 接收发布 | /data (默认) | /data |

其中：
- `<id>`: link_map 查找ID，对应目标 IP
- `<proto>`: 1=UDP, 2=TCP
- `<topic>`: VSOA publish 主题路径

---

## 验证方法

### 1. VSOA RPC 测试

```bash
# 发送 VSOA RPC
vsoa_client send --url "/toLink/89/1/topic/data" --data '{"test":123}'

# 预期结果：
# 722device 通过 UDP 发送到 192.168.1.100:8888
```

### 2. TCP 测试

```bash
# TCP 客户端发送
echo '{"temp":25.5}' | nc localhost 9000

# 预期结果：
# 722device 通过 VSOA publish 发送 /data
```

### 3. UDP 测试

```bash
# UDP 客户端发送
echo '{"humidity":60}' | nc -u localhost 9001

# 预期结果：
# 722device 通过 VSOA publish 发送 /data
```

---

## 修改文件清单

| 文件 | 修改类型 | 说明 |
|------|----------|------|
| include/cJSON.h | 新增 | cJSON 库头文件 |
| include/cJSON.c | 新增 | cJSON 库实现 |
| network/tls_net_proxy.h | 修改 | 新增 app_vsoa_publish_t 前向声明，新增 tls_net_proxy_set_publish |
| network/tls_net_proxy.c | 重写 | cJSON 解析，TCP/UDP 直接发布，发送线程 |
| app/app_transmit.c | 修改 | 使用 tls_net_proxy_load_config_json 和 tls_net_proxy_set_publish |
| MODIFICATION_DOC.md | 重写 | 更新为 v3.0 文档 |

---

## 后续优化

1. **优先级扩展**：URL 中添加优先级字段解析
2. **TCP 连接池**：避免频繁创建连接
3. **发送重试**：失败后加入重试队列
4. **流量控制**：根据优先级进行流量整形