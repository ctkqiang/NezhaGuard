# 编译器与标准
CXX       = g++
CXXFLAGS  = -std=c++20 -Wall -Wextra -O2
LDFLAGS   =   # 不再需要 -lstdc++fs

# 目标可执行文件名
TARGET    = nezha_guard

# 源文件目录（自动收集所有 .cc）
SRC_DIR   = .
SRCS      = $(wildcard *.cc) $(wildcard src/**/*.cc)
OBJS      = $(SRCS:.cc=.o)

# 头文件目录
INCLUDES  = -I.

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 编译规则
%.o: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

# 运行
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run