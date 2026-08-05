# coServerFramework

一个使用 **现代 C++20** 从零重写的 **Linux 服务器开发框架** —— 对 [sylar](https://github.com/sylar-yin/sylar) 框架（Bilibili 知名课程 *《[C++高级教程]从零开始开发服务器框架》*）的独立实现。

核心执行模型为 **协程（Fiber）+ 协程调度器（Scheduler）+ 基于 epoll 的 IO 管理器（IOManager）+ 系统调用 Hook**，让阻塞式 socket/sleep 调用在协程上看起来像同步代码，同时不阻塞线程。

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-%3E%3D3.16-064F8C.svg)](https://cmake.org)
[![Linux](https://img.shields.io/badge/Platform-Linux-FCC624.svg)](https://www.kernel.org/)

---

## 项目特点

- **并发核心完整自研**：`Fiber`（ucontext 用户态协程）+ `Scheduler`（N-M 协程线程池）+ `IOManager`（epoll + 毫秒级定时器轮）+ `Hook`（dlsym 拦截 socket/IO/sleep），四者构成一个可独立运行的并发底座。
- **网络层全覆盖**：统一 `Address`（IPv4/IPv6/Unix）、`Socket`/`SSLSocket`、`TcpServer`、`Stream` 抽象（SocketStream / ZlibStream / 异步流 / 负载均衡 / 服务发现）。
- **HTTP/1.1 + WebSocket**：**手写增量解析器**（替代 ragel 生成），HTTP 服务端/客户端/连接池、Servlet 分发（精准 + 通配匹配）、WebSocket 服务端/客户端完整实现。
- **数据库与中间件封装**：MySQL（预编译语句、连接池）、Redis / RedisCluster（同步 + FoxThread 异步）、SQLite3、FoxThread 线程池、Zookeeper 客户端。
- **丰富的数据结构库**：Bitmap、RoaringBitmap（CRoaring 位图）、Dict、HashMap / Multimap、LRU / 定时缓存、Array 等。
- **应用层一站式**：YAML 驱动配置、`Application` 生命周期（daemon 模式、pid 文件、模块回调、worker 池、SSL 证书、ZK 服务发现）、动态 `.so` 模块加载。
- **现代 C++ 风格**：RAII、智能指针、`std::format`、concept、`#pragma once`、无裸 `new`/`delete`、统一命名规范。

---

## 架构

```
                    Application（YAML 配置、daemon、模块、多类型服务器）
                                      │
      ┌─────────────┬──────────────┬──┴───┬───────────────┬───────────────┐
   HTTP / WS     Rock / NS     TcpServer   email            orm 生成器
      └─────────────┴──────────────┼──────┴───────────────┴───────────────┘
                       Socket / Address / Stream 抽象层
                                      │
            ┌─────────────────────────┼─────────────────────────┐
      Hook (dlsym 拦截)          IOManager (epoll+定时器)    ByteArray 序列化
            │                         │
               Scheduler（N-M 协程线程池）
                         │
            Fiber (ucontext) + Thread / Mutex / RWMutex / Spinlock
```

### 核心模块

| 模块 | 作用 |
|------|------|
| `log` | 流式 + printf 风格日志，级别/格式/多 Logger 自由配置 |
| `config` | "定义即用" YAML 配置：`Config::lookup("a.b.c", 默认值, 描述)`，变更回调，STL/自定义类型支持 |
| `thread` | 封装 pthread（`Thread`/`Mutex`/`RWMutex`/`Spinlock`/`Semaphore`） |
| `fiber` | 基于 ucontext 的用户态协程 |
| `scheduler` | N-M 协程调度：N 线程，M 协程，可指定线程执行 |
| `iomanager` | 协程调度器 + epoll + 毫秒级定时器（一次性/循环/条件） |
| `hook` | 线程粒度拦截 socket/IO/sleep，使阻塞 API 具备异步能力 |
| `socket`/`address` | 统一 IPv4/IPv6/Unix 地址，`Socket` + `SSLSocket`，域名解析 |
| `bytearray` | 二进制序列化：varint、字节序、模板 `read<T>()`/`write<T>()` |
| `tcp_server` | 通用 TCP 服务器基类（bind/listen/accept/handleClient） |
| `stream`/`streams` | 统一 `Stream` 接口：SocketStream、ZlibStream、异步流、负载均衡、服务发现 |
| `http` | 手写 HTTP/1.1 + URI 解析器、HttpServer、HttpSession、HttpConnection + 连接池、WebSocket 客户端/服务端、Servlet 分发 |
| `db` | MySQL、Redis(+Cluster/FoxThread)、SQLite3、FoxThread 线程池 |
| `datastruct` | Bitmap、RoaringBitmap、Dict、HashMap/Multimap、LRU/定时缓存、Array |
| `ns`/`rock` | 名字服务 / Rock 协议（protobuf）、服务发现、RockServer |
| `orm` | 表/列映射 + `orm` 代码生成器（XML → C++） |
| `email` | 邮件实体 + SMTP 客户端（SSL、AUTH LOGIN、附件） |
| `application` | 完整应用生命周期：daemon、pid 文件、模块、worker、SSL 证书、ZK 服务发现 |

---

## 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | **C++20**（`std::format`、concept、结构化绑定、`if constexpr`） |
| 构建 | CMake ≥ 3.16 + **vcpkg** 依赖管理，多 preset |
| 并发 | ucontext 协程、pthread 封装、原子操作（GCC `__sync`）、epoll |
| Hook | dlsym / ELF 符号拦截 |
| 网络协议 | 手写 HTTP/1.1、WebSocket RFC6455、URI 解析、Rock 二进制协议 |
| 序列化 | 自研 ByteArray + **Google Protobuf**（ns/rock） |
| 数据库 | MySQL C API、hiredis-vip（Redis/Cluster）、SQLite3 |
| 压缩/安全 | zlib（gzip/deflate）、OpenSSL（AES/RSA、SSL/TLS） |
| 配置 | yaml-cpp、jsoncpp、tinyxml2（orm） |
| 事件/服务 | libevent（FoxThread、异步 Redis）、Zookeeper C 客户端 |
| 数据结构 | 自研 Bitmap + **CRoaring** 位图 |

---

## 技术亮点

1. **协程 + Hook 的"假同步"模型**
   基于 ucontext 实现用户态协程，配合 dlsym 对 `read/write/send/recv/sleep` 等系统调用做线程粒度拦截：业务代码按同步方式编写，底层却在 epoll 上异步调度，把"异步性能、同步心智"落到了实处。

2. **手写增量 HTTP/1.1 解析器**
   不依赖 ragel 生成器，改为分块输入、状态机推进的手写解析器，请求行 → 头部 → 正文（Content-Length）逐段消费，支持粘包/半包，代码量更小、更易读、易调试。

3. **统一 Stream 抽象**
   将 Socket、Zlib 压缩、异步连接全部收敛到 `Stream` 接口，上层（HTTP、Rock、负载均衡）只面向接口编程，可以任意嵌套组合（例如「HTTP over SSL over Socket」）。

4. **模板驱动的二进制序列化**
   `ByteArray` 用模板 `write<T, ByteSize>()`/`read<T, ByteSize>()` 统一了定长/变长（Varint）编码，消除了一族 `writeInt/writeFint/writeStringVint` 重复接口。

5. **连接池与资源复用**
   `HttpConnectionPool` 管理连接的生命周期（最大存活时间、最大请求数、失效回收），配合 keep-alive 显著降低握手开销；MySQL 同样提供连接池。

6. **N-M 调度与级联唤醒**
   调度器按"线程池 + 协程队列"工作；IOManager 停止时通过事件管道级联唤醒 epoll_wait，避免停机停摆（本项目修过此坑）。

7. **完整应用开箱即用**
   `Application` 一条命令拉起：读 YAML → 建 pid/工作目录 → 初始化模块/worker/缓存 → 按类型（http/ws/rock/nameserver）绑定并启动多服务器 → 可选接入 ZK 服务发现，注册自身为服务节点。

---

## 设计思路

本项目不是对原框架的逐行拷贝，而是**用现代 C++20 的思维重写一遍**：

- **分层递进，每一层只解决一个问题**：先有日志/配置打底 → 线程 → 协程 → 调度器 → IO 管理器 → Hook → 网络 → HTTP → 应用。每一层都尽量薄，上层只依赖下层的接口。
- **优先 RAII 与智能指针**：资源（栈、fd、SSL 上下文、zookeeper handle）全部由智能指针 + RAII 管理，杜绝裸 `new`/`delete` 泄漏。
- **接口统一、命名规范**：成员 `_lowerCamelCase`、类型 `PascalCase`、自由函数 `lowerCamelCase`、常量 `SCREAMING_SNAKE`；`#pragma once`；clang-format 统一格式。
- **能编译进测试的就测试**：每个模块对应 `tests/test_*` 独立可执行（无测试框架、纯 assert），同时保留对 MySQL/Redis/SMTP 的真实服务集成测试（凭据走 gitignore 配置）。

---

## 构建

依赖较多，通过 **vcpkg** 安装，推荐使用 CMake preset 流程（`$VCPKG_ROOT` 指向 vcpkg 目录）：

```sh
cmake --preset debug          # 配置到 build/debug（vcpkg toolchain）
cmake --build --preset debug  # 编译库 + 全部测试/示例
```

产物：静态库在 `lib/`，可执行文件在 `bin/`。

## 运行

```sh
./bin/azzatoBackend -s                    # 前台启动，读取 bin/conf/server.yml
./bin/azzatoBackend -s -c <配置目录>       # 指定配置目录
```

`server.yml` 示例（启动一个监听 `0.0.0.0:8020` 的 HTTP 服务器）：

```yaml
server:
    work_path: /tmp/azzato_work
    pid_file: azzato.pid

servers:
    - name: azzato_http
      type: http
      address:
          - 0.0.0.0:8020
      keepalive: 1
      timeout: 120000
```

## 测试

无测试框架 —— 每个 `tests/test_*.cpp` 都是自带 `main()` 的独立可执行（纯 `assert` + 日志输出）：

```sh
ctest --test-dir build/debug               # 运行全部 29 个测试
./bin/test_http_server                     # 单独运行某个测试
```

- **离线测试**覆盖并发核心、网络层、HTTP/WS、数据结构、配置等全部模块。
- **集成测试**（`test_mysql`/`test_redis`/`test_email`）从 **gitignore 的** `bin/conf/services.yml` 读取真实中间件凭据（不落库），文件缺失时优雅跳过。

---

## 代码规范

- 头文件放 `include/`，实现放 `src/`，按模块建同名子目录
- `#pragma once`，不用 include guard 宏
- 类型 `PascalCase`、成员 `_lowerCamelCase`、函数 `lowerCamelCase`、常量 `SCREAMING_SNAKE`
- 优先 RAII / 智能指针 / `const&`；用 `static_cast` 而非 C 风格强转
- 全库用 `format_cpp_files`（clang-format）统一格式

---

## 学习路径

本项目同时作为**学习代码库**使用，建议按以下顺序阅读，与原课程节奏一致：

1. 基础三件套：`log` → `config` → `env`，再到 `thread`
2. 并发核心：`fiber` → `scheduler` → `iomanager` → `hook`（理解"协程 + 事件 + 拦截"如何配合）
3. 网络层：`address` → `socket` → `tcp_server` → `stream` → `streams`
4. HTTP：解析器 → 服务端 → 客户端/连接池 → WebSocket → Servlet 分发
5. 扩展模块：`db` → `datastruct` → `email` → `ns`/`rock` → `orm` → `application`

---

## 致谢

- 感谢 [**sylar（sylar-yin）**](https://github.com/sylar-yin/sylar) 开源了这套影响深远的服务器框架，以及配套的 [《从零开始开发服务器框架》视频教程](https://www.bilibili.com/video/av53602631/)，为国内 C++ 服务端学习者提供了极佳的入门与进阶素材。
- 感谢课程视频的**讲解思路**与**逐模块演进方式**，本项目的模块划分与学习路径深受其启发。
- 感谢 **sylar 及众多网友**在各种技术论坛中的讨论/补充与二次实现，帮助我厘清了许多实现细节。
- 本项目的字节流、HTTP/WebSocket、MySQL/Redis 等封装，均是在研读 sylar 源码的基础上，用现代 C++20 重新设计与实现。
