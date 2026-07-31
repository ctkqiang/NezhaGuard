# 哪吒网络安全 SIEM 系统

> Qt6 / C++26 实时安全信息与事件管理系统 — 蓝队主动防御平台

![系统架构](docs/images/哪吒网络安全SIEM系统架构.png)

## 核心功能

| 模块 | 功能 | 技术实现 |
|------|------|----------|
| 抓包引擎 | 实时网络流量采集 | libpcap, en0 混杂模式 |
| 协议解码 | Ethernet / IPv4 / IPv6 / TCP / UDP / ICMP / HTTP | ProtocolDecoder |
| 蜜罐监听 | 8 端口诱饵 (SSH, Telnet, MySQL, Redis 等) | HoneypotListener |
| 日志监控 | 系统日志实时采集 (nginx, apache, auth, syslog) | LogWatcher |
| 攻击检测 | 85+ 攻击签名 + 速率异常检测 | AttackDetector |
| Tor 检测 | 识别 Tor 出口节点流量 (1365+ 节点) | TorChecker |
| GeoIP | IP 地理位置 (国家/城市/坐标/ISP/时区) | ip-api.com |
| 隔离系统 | SQLite 持久化 + 内存 O(1) 哈希查询 | DatabaseHelper |
| 主动响应 | ICMP Unreachable / TCP RST 强制断开 | ActiveResponse |
| GUI 仪表盘 | Qt6 Miku Cyan 双主题实时面板 | monitor |
| CLI 控制台 | 蓝队模式终端日志 | Logger |

## 检测能力

### 攻击签名

| 类别 | 检测模式 | 级别 |
|------|----------|------|
| SQL 注入 | `UNION SELECT`, `SLEEP()`, `' OR 1=1`, `DROP TABLE` | Critical / Error |
| XSS | `<script>`, `onerror=`, `document.cookie` | Critical / Error |
| 路径穿越 | `/etc/passwd`, `....//`, `../` | Critical / Error |
| 命令注入 | `;wget`, `$(whoami)`, `\| /bin/bash` | Critical / Error |
| 文件包含 | `=http://`, `php://input` | Critical / Error |
| 扫描探测 | `nmap`, `sqlmap`, `nikto`, `wp-login.php` | Error / Warn |
| Webshell | `eval(base64_decode`, `system($_` | Critical |
| Log4j | `${jndi:ldap://`, `${jndi:dns://` | Critical |
| 恶意爬虫 | `AhrefsBot`, `SemrushBot`, `DotBot` | Warn |

### 速率检测

| 协议 | 触发阈值 | 告警级别 | 自动隔离阈值 |
|------|----------|----------|-------------|
| ICMP | ≥ 5 次 | Info → Warn → Error → Critical | ≥ 100 次 |
| TCP/UDP | ≥ 100 次 | Warn → Error → Critical | — |

## 数据流

![检测流程](docs/images/哪吒网络安全SIEM检测流程.png)

### 启动流程

![启动时序](docs/images/哪吒网络安全SIEM启动流程.png)

### 隔离与主动响应

![隔离流程](docs/images/哪吒网络安全SIEM隔离与主动响应.png)

### GUI 数据流

![GUI数据流](docs/images/哪吒网络安全SIEM%20GUI数据流.png)

## 日志格式

```
2026-07-31 14:44:27  [哪吒] [INFO ]  SIEM 引擎已启动
2026-07-31 14:44:28  [哪吒] [WARN ]  已隔离 IP 被拦截: 192.168.1.75
2026-07-31 14:44:29  [哪吒] [CRIT ]  告警 — SQL注入 | 43.156.223.237 | 频次 12 | 评分 90
```

## 构建

### 依赖

| 依赖 | 用途 |
|------|------|
| Qt 6.5+ (Widgets, Concurrent) | GUI 框架 |
| libpcap | 网络抓包 |
| SQLite 3 | 隔离与缓存数据库 |
| CMake 4.3+ | 构建系统 |
| Apple Clang 17+ / GCC 14+ | C++26 编译器 |

### 编译

```bash
brew install qt6 libpcap sqlite3

# Debug
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) -G Ninja
cmake --build cmake-build-debug -j$(sysctl -n hw.ncpu)

# Release
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt6) -G Ninja
cmake --build cmake-build-release -j$(sysctl -n hw.ncpu)
```

### 运行

```bash
# GUI 模式
sudo open cmake-build-debug/NezhaGuard.app

# CLI 蓝队模式 (ShowGui = false)
sudo ./cmake-build-debug/NezhaGuard.app/Contents/MacOS/NezhaGuard

# 详细模式
sudo ./NezhaGuard -v
```

## 项目结构

```
NezhaGuard/
├── main.cc                          # 主入口 CLI / GUI 双模式
├── CMakeLists.txt                   # CMake 构建
├── src/
│   ├── contants.h                   # 全局常量
│   ├── Info.plist                   # macOS Bundle
│   ├── core/                        # 核心引擎
│   │   ├── alert.h/cc               # 告警管理 (去重 / 发射)
│   │   ├── arena.h/cc               # 内存池 128KB 块
│   │   ├── capture.h/cc             # libpcap 抓包
│   │   ├── decoder.h/cc             # 协议解码器
│   │   ├── detector.h/cc            # 攻击检测引擎 (85+ 签名)
│   │   ├── event.h/cc               # 归一化事件
│   │   ├── honeypot.h/cc            # 蜜罐监听器
│   │   ├── ipaddr.h/cc              # IPv4/IPv6 统一地址
│   │   ├── log_watcher.h/cc         # 日志文件监控
│   │   ├── net_util.h/cc            # ARP / 接口信息
│   │   ├── tor_checker.h/cc         # Tor 出口节点
│   │   ├── geo_ip.h/cc              # IP 地理位置
│   │   ├── active_response.h/cc     # 主动响应
│   │   └── types.h                  # 基础类型
│   ├── service/
│   │   └── database_helper.h/cc     # SQLite 隔离
│   ├── utilities/
│   │   └── logger.h/cc              # 日志系统
│   ├── views/
│   │   ├── monitor.h/cc             # Qt6 主窗口
│   │   ├── monitor.ui               # Qt Designer 布局
│   │   ├── log_model.h/cc           # QAbstractListModel
│   │   ├── gui_sink.h/cc            # ISink → GUI
│   │   ├── app_icon.svg             # App 图标
│   │   └── app_icon.icns            # macOS Bundle 图标
│   └── model/
│       ├── request.h
│       └── severity.h
├── docs/
│   ├── UML/                         # PlantUML 源文件
│   └── images/                      # 导出 PNG 图
└── logs/                            # 日志输出 (.gitignore)
```

## 技术特性

| 特性 | 实现 |
|------|------|
| 内存池 | Arena 128KB 块分配器 |
| 隔离查询 | `unordered_set` O(1) 内存缓存 + SQLite 持久化 |
| Tor 检测 | 内存缓存 + 本地文件缓存 + 后台自动刷新 |
| GUI 日志 | 5000 条环形缓冲区, QSortFilterProxyModel 过滤 |
| DNS/GeoIP | QtConcurrent 异步 (不阻塞 GUI 线程) |
| 消息去重 | 拦截警告 10 秒冷却, 告警 10 秒聚合窗口 |
| 主动响应 | ICMP Type3 Code1 / TCP RST+ACK 原始套接字 |
| 双主题 | 自动跟随系统亮暗模式切换 |

### 告警去重参数

| 参数 | 值 |
|------|-----|
| 去重窗口 | 10 秒 |
| 聚合维度 | 攻击类型 + 源 IP |
| 最高级别 | 窗口内取最大值 |

## 许可证

Copyright © 2026 钟智强
