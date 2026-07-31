FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build g++ \
    qt6-base-dev libpcap-dev libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja \
    && cmake --build build -j$(nproc)

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV NEZHA_SHOW_GUI=0

LABEL authors="钟智强"
LABEL org.opencontainers.image.title="哪吒网络安全 SIEM"
LABEL org.opencontainers.image.description="蓝队主动防御 SIEM 系统"
LABEL org.opencontainers.image.version="v0.0.1"

RUN apt-get update && apt-get install -y --no-install-recommends \
    qt6-base libpcap0.8 libsqlite3-0 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/build/NezhaGuard /app/NezhaGuard

RUN mkdir -p /app/logs /app/data

VOLUME ["/app/logs", "/app/data"]

ENTRYPOINT ["/app/NezhaGuard"]
CMD ["-v"]
