# ============================================================
# Net Proxy V2 - 跨网段数据转发系统
# ============================================================
#
# 目录结构:
#   common/       - 公共库和工具函数
#   proxy_server/ - 代理服务器(程序1和程序2)
#   client_a/     - 测试客户端A
#   subscriber_b/ - VSOA订阅者B
#
# 使用方法:
#   程序1(192.168网段): ./proxy_server 1
#   程序2(10.10.111网段): ./proxy_server 2
#   客户端A: ./client_a [目标IP] [端口] [协议]
#   订阅者B: ./subscriber_b [主题名]
#
# 作者: Claude Code
# 日期: 2026-04-28

CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -pthread -std=c11
LDFLAGS = -pthread

# 公共库
COMMON_DIR = common
COMMON_LIB = $(COMMON_DIR)/libcommon.a
COMMON_OBJ = $(COMMON_DIR)/common.o

# 程序
PROXY_DIR = proxy_server
CLIENT_DIR = client_a
SUBSCRIBER_DIR = subscriber_b

# 可执行文件输出目录
BIN_DIR = bin
PROXY_BIN = $(BIN_DIR)/proxy_server
CLIENT_BIN = $(BIN_DIR)/client_a
SUBSCRIBER_BIN = $(BIN_DIR)/subscriber_b

# 默认目标
all: $(BIN_DIR) $(COMMON_LIB) $(PROXY_BIN) $(CLIENT_BIN) $(SUBSCRIBER_BIN)

# ============================================================
# 公共库构建
# ============================================================

$(COMMON_LIB): $(COMMON_OBJ)
	ar rcs $@ $^

$(COMMON_OBJ): $(COMMON_DIR)/common.c $(COMMON_DIR)/common.h
	$(CC) $(CFLAGS) -c $(COMMON_DIR)/common.c -o $@

# ============================================================
# Proxy Server 构建
# ============================================================

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(PROXY_BIN): $(PROXY_DIR)/proxy_server.o $(COMMON_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(PROXY_DIR)/proxy_server.o $(COMMON_LIB) $(LDFLAGS)

$(PROXY_DIR)/proxy_server.o: $(PROXY_DIR)/proxy_server.c $(PROXY_DIR)/proxy_server.h $(COMMON_DIR)/common.h
	$(CC) $(CFLAGS) -c $(PROXY_DIR)/proxy_server.c -o $@

# ============================================================
# Client A 构建
# ============================================================

$(CLIENT_BIN): $(CLIENT_DIR)/client_a.o $(COMMON_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_DIR)/client_a.o $(COMMON_LIB) $(LDFLAGS)

$(CLIENT_DIR)/client_a.o: $(CLIENT_DIR)/client_a.c $(COMMON_DIR)/common.h
	$(CC) $(CFLAGS) -c $(CLIENT_DIR)/client_a.c -o $@

# ============================================================
# Subscriber B 构建
# ============================================================

$(SUBSCRIBER_BIN): $(SUBSCRIBER_DIR)/subscriber_b.o $(COMMON_LIB) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(SUBSCRIBER_DIR)/subscriber_b.o $(COMMON_LIB) $(LDFLAGS)

$(SUBSCRIBER_DIR)/subscriber_b.o: $(SUBSCRIBER_DIR)/subscriber_b.c $(COMMON_DIR)/common.h
	$(CC) $(CFLAGS) -c $(SUBSCRIBER_DIR)/subscriber_b.c -o $@

# ============================================================
# 测试目标
# ============================================================

test: all
	@echo ""
	@echo "========================================="
	@echo "    Build Complete - Test Targets"
	@echo "========================================="
	@echo ""
	@echo "To run the system, start components in order:"
	@echo ""
	@echo "1. On 10.10.111.x network:"
	@echo "   $(SUBSCRIBER_BIN)"
	@echo ""
	@echo "2. On 10.10.111.x network:"
	@echo "   $(PROXY_BIN) 2"
	@echo ""
	@echo "3. On 192.168.x network:"
	@echo "   $(PROXY_BIN) 1"
	@echo ""
	@echo "4. On 192.168.x network:"
	@echo "   $(CLIENT_BIN) 192.168.3.10 8888 tcp"
	@echo ""
	@echo "========================================="

# ============================================================
# 清理
# ============================================================

clean:
	rm -rf $(BIN_DIR)
	rm -f $(COMMON_LIB) $(COMMON_OBJ)
	rm -f $(PROXY_DIR)/*.o
	rm -f $(CLIENT_DIR)/*.o
	rm -f $(SUBSCRIBER_DIR)/*.o
	rm -f *~ */*.o */*~

# ============================================================
# 静态检查
# ============================================================

static-check:
	@echo "Running static analysis..."
	gcc -Wall -Wextra -pedantic -std=c11 -fanalyzer -o $(PROXY_BIN) $(PROXY_DIR)/proxy_server.c $(COMMON_DIR)/common.c 2>&1 || true
	@echo "Note: If fanalyzer is not available, install gcc with analysis support"

# ============================================================
# 伪目标
# ============================================================

.PHONY: all clean test static-check