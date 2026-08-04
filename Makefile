# 哪吒网络安全 SIEM — 跨平台 Makefile
#    make        → 编译
#    make run    → 编译并运行
#    make clean  → 清理构建产物
# 无需额外脚本，clone 即可用

BUILD_DIR := build
BIN       := $(BUILD_DIR)/NezhaGuard
CMAKE     := cmake

# ---- 平台检测 ----
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME_S),Darwin)
  # macOS — GUI 模式
  CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DCLI_ONLY=OFF
  RUN_PREFIX  :=
else ifeq ($(UNAME_S),Linux)
  # Linux — CLI 无界面模式
  CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DCLI_ONLY=ON
  RUN_PREFIX  := sudo
else
  # Windows — GUI 模式
  CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=Release -DCLI_ONLY=OFF
  RUN_PREFIX  :=
  BIN         := $(BUILD_DIR)/NezhaGuard.exe
endif

# ---- 生成器选择 ----
GENERATOR := $(shell $(CMAKE) --help 2>/dev/null | grep -q Ninja && echo Ninja || echo "Unix Makefiles")
ifeq ($(GENERATOR),Ninja)
  CMAKE_FLAGS += -G Ninja
endif

# ---- 目标 ----
.PHONY: all build run clean

all: build

build:
	@$(CMAKE) -B $(BUILD_DIR) $(CMAKE_FLAGS)
	@$(CMAKE) --build $(BUILD_DIR)

run: build
	$(RUN_PREFIX) ./$(BIN) -v

clean:
	@rm -rf $(BUILD_DIR)
	@echo "已清理 $(BUILD_DIR)/"