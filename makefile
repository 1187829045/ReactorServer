# 编译器和参数
CXX = g++
CXXFLAGS = -Wall -std=c++11 -Iinet_address    # ✅ 指定头文件目录

# 目录
CLIENT_DIR = client
SERVER_DIR = server
INET_DIR = inet_address

BUILD_DIR = build

# 源文件
CLIENT_SRC = $(CLIENT_DIR)/client.cpp
SERVER_SRC = $(SERVER_DIR)/tcpepoll.cpp $(INET_DIR)/inet_address.cpp   # ✅ 添加 inet_address 源文件

# 可执行文件名
CLIENT_EXEC = client_exec
SERVER_EXEC = server_exec

# 默认目标
all: $(CLIENT_EXEC) $(SERVER_EXEC)

# 创建 build 目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译 client
$(CLIENT_EXEC): $(CLIENT_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $<

# 编译 server（多个源文件）
$(SERVER_EXEC): $(SERVER_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^     # ✅ 使用 $^ 表示多个源文件

# 清理
clean:
	rm -rf $(BUILD_DIR) $(CLIENT_EXEC) $(SERVER_EXEC)
