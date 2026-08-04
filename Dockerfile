# 哪吒网络安全 SIEM — Docker 镜像 (CLI / Headless)
# 用法:
#   docker build -t nezhaguard .
#   docker run --rm --net=host --cap-add=NET_RAW --cap-add=NET_ADMIN \
#       -v ./logs:/app/logs -v ./data:/app/data \
#       -v ./config:/etc/nezhaguard/config -v ./rules:/etc/nezhaguard/rules \
#       -e NEZHA_INTERFACE=eth0 nezhaguard

# ============================================================
# 构建阶段
# ============================================================
FROM ubuntu:plucky AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-14 cmake ninja-build \
    libpcap-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-14 CXX=g++-14

WORKDIR /build
COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCLI_ONLY=ON \
    && cmake --build build -j$(nproc) \
    && cmake --install build --prefix /install

# ============================================================
# 运行阶段
# ============================================================
FROM ubuntu:plucky

ENV DEBIAN_FRONTEND=noninteractive
ENV NEZHA_SHOW_GUI=0

LABEL authors="钟智强"
LABEL org.opencontainers.image.title="哪吒网络安全 SIEM"
LABEL org.opencontainers.image.description="蓝队主动防御 SIEM 系统 — CLI/Headless"
LABEL org.opencontainers.image.version="v0.0.1"

RUN apt-get update && apt-get install -y --no-install-recommends \
    libpcap0.8t64 libsqlite3-0 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /install/bin/NezhaGuard /app/nezhaguard
COPY --from=builder /etc/nezhaguard /etc/nezhaguard

RUN mkdir -p /app/logs /app/data

WORKDIR /app

VOLUME ["/app/logs", "/app/data"]

# 容器内网口（可被 NEZHA_INTERFACE 覆盖）
ENV NEZHA_INTERFACE=eth0

ENTRYPOINT ["/app/nezhaguard"]
CMD ["-v"]