# azzatoBackend

A modern **C++20 server framework** — a from-scratch rewrite of the [sylar](https://github.com/sylar-yin/sylar) Linux server framework (famous from the Bilibili course *[C++高级教程]从零开始开发服务器框架*).

Built on a **coroutine + scheduler + epoll I/O manager + syscall hook** core, it makes blocking socket/sleep calls look synchronous while running on user-space fibers — the classic N-M coroutine-thread model.

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-%3E%3D3.16-064F8C.svg)](https://cmake.org)

---

## Highlights

- **Concurrency core** — `Fiber` (ucontext) + `Scheduler` (N-M thread pool) + `IOManager` (epoll + ms-precision timer wheel) + `Hook` (dlsym interception of socket/IO/sleep).
- **Full networking stack** — unified `Address` types, `Socket`/`SSLSocket`, `TcpServer`, `Stream` abstractions.
- **HTTP/1.1 + WebSocket** — hand-written incremental parser (no ragel), server/client/connection-pool, servlet dispatch (exact + glob), WebSocket server + client.
- **Databases** — MySQL (prepared statements, connection pool), Redis/RedisCluster (sync + FoxThread async), SQLite3.
- **Data structures** — bitmap, roaring bitmap, dict, hash map/multimap, LRU / timed caches, array.
- **App layer** — YAML-driven `Application` lifecycle, dynamic `.so` module loading, daemon mode, ZK service discovery + load balancing.
- **Modern C++** — RAII, smart pointers, `std::format`, concepts, `#pragma once`, strict naming conventions (see [Style](#style)).

---

## Architecture

```
                      Application (YAML config, daemon, modules, servers)
                                      │
        ┌─────────────┬──────────────┼──────────────┬───────────────┐
     HTTP/WS       Rock/NS        TcpServer      email            orm
        └─────────────┴──────────────┼──────────────┴───────────────┘
                              Socket / Address / Stream
                                      │
                 ┌────────────────────┼────────────────────┐
          Hook (dlsym)           IOManager (epoll)     ByteArray
                 │                    │
              Scheduler (N-M coroutine thread pool)
                 │
              Fiber (ucontext) + Thread / Mutex / RWMutex / Spinlock
```

### Core modules

| Module | What it does |
|--------|--------------|
| `log` | Streaming + printf-style logging, configurable levels/formats, per-logger config |
| `config` | Define-and-use YAML config: `Config::lookup("a.b.c", default, "desc")`, change callbacks, STL/custom types |
| `thread` | `pthread` wrappers (`Thread`, `Mutex`, `RWMutex`, `Spinlock`, `Semaphore`) |
| `fiber` | User-space coroutines on `ucontext_t` |
| `scheduler` | N-M coroutine-thread pool (N threads, M coroutines) |
| `iomanager` | `Scheduler` + epoll + ms timer wheel (one-shot/repeating/conditional timers) |
| `hook` | Thread-granular interception of socket/IO/`sleep` so blocking calls run on fibers |
| `socket` / `address` | Unified IPv4/IPv6/Unix addresses, `Socket` + `SSLSocket`, DNS resolution |
| `bytearray` | Binary serialization: varint, endian, `read<T>()`/`write<T>()` |
| `tcp_server` | Reusable TCP server base (`bind`/`listen`/`accept`/`handleClient`) |
| `stream` / `streams` | Unified `Stream` interface: `SocketStream`, `ZlibStream`, `AsyncSocketStream`, `LoadBalance`, `ServiceDiscovery` |
| `http` | Hand-written HTTP/1.1 + URI parsers, `HttpServer`, `HttpSession`, `HttpConnection` + `HttpConnectionPool`, WebSocket server/client, servlet dispatch |
| `db` | MySQL, Redis(+Cluster, FoxThread async), SQLite3, `fox_thread` libevent thread pool |
| `datastruct` | Bitmap, RoaringBitmap, Dict, HashMap/Multimap, Lru/TimedCache, Array |
| `ns` / `rock` | Nameserver / Rock protocol (protobuf), service discovery, `RockServer` |
| `orm` | Table/column mapping + `orm` code generator (XML → C++) |
| `email` | Email entity + SMTP client (SSL, AUTH LOGIN, attachments) |
| `application` | Full app lifecycle: daemon, pid file, modules, workers, SSL certs, ZK discovery |

---

## Requirements

- Linux, **C++20** compiler (GCC ≥ 11 / Clang ≥ 14)
- [vcpkg](https://github.com/microsoft/vcpkg) toolchain (`$VCPKG_ROOT`), plus system Boost/OpenSSL/ZLIB, MySQL client, and a compiled `hiredis-vip`
- Dependencies: yaml-cpp, jsoncpp, openssl, zlib, Boost, sqlite3, mysqlclient, hiredis-vip, libevent, protobuf, zookeeper, tinyxml2

## Build

```sh
cmake --preset debug            # configure into build/debug (vcpkg toolchain)
cmake --build --preset debug    # build lib + all test/example binaries
```

Binaries land in `bin/`, the static library in `lib/`.

## Run

```sh
./bin/azzatoBackend -s                      # start foreground with bin/conf/server.yml
./bin/azzatoBackend -s -c <conf_dir>        # custom config dir
```

`server.yml` starts an HTTP server on `0.0.0.0:8020`:

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

## Testing

There is no test framework — each `tests/test_*.cpp` is a standalone executable with its own `main()` (plain `assert` + log output).

```sh
ctest --test-dir build/debug                # run all 29 tests
./bin/test_http_server                      # run one test
```

Offline tests cover the concurrency core, networking, HTTP/WS, data structures, config, and more. **Integration tests** (`test_mysql`, `test_redis`, `test_email`) read real-service credentials from a **gitignored** `bin/conf/services.yml` (never committed) and skip gracefully if it's absent.

## Style

- `include/` headers, `src/` sources; module-name subdirectories
- `#pragma once`, no include guards
- Types `PascalCase`, members `_lowerCamelCase`, functions `lowerCamelCase`, constants `SCREAMING_SNAKE`
- RAII / smart pointers / `const&`; `static_cast` over C-style casts
- C++20: `std::format`, concepts, `<format>` where available
- Format all sources with `format_cpp_files` (clang-format)

## Learning path

This checkout doubles as a **study codebase**. A good reading order mirrors the original course:

1. `log` → `config` → `env` → `thread`
2. `fiber` → `scheduler` → `iomanager` → `hook`
3. `address` → `socket` → `tcp_server` → `stream` → `streams`
4. `http` (parser → server → connection → websocket → servlet)
5. `db` → `datastruct` → `email` → `ns`/`rock` → `orm` → `application`

## Related

- Original framework & course: [sylar-yin/sylar](https://github.com/sylar-yin/sylar) · [Bilibili course](https://www.bilibili.com/video/av53602631/)
- This project is an independent rewrite: modern C++20 conventions, hand-written HTTP parser, cleaner build.
