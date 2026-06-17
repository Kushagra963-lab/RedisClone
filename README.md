# RedisClone

RedisClone is a small Redis-inspired key-value database written in C++20. It supports an interactive console, a TCP server for `telnet`/`nc` clients, key expiration, persistence, runtime stats, tests, and a simple benchmark.

## Features

- Thread-safe in-memory store using `std::unordered_map`
- TCP server on port `6379` by default
- One worker thread per connected client
- Background cleanup for expired keys
- Snapshot persistence through `SAVE`
- Runtime `INFO` command
- CMake project, Dockerfile, tests, and benchmark

## Commands

```text
SET <key> <value>
GET <key>
DEL <key>
EXISTS <key>
EXPIRE <key> <seconds>
KEYS
SAVE
INFO
HELP
QUIT
```

Values with spaces can be wrapped in quotes:

```text
SET greeting "hello world"
```

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If CMake is unavailable, you can compile the main server directly:

```bash
c++ -std=c++20 -pthread -Iinclude \
  src/main.cpp src/command_executor.cpp src/command_parser.cpp src/datastore.cpp src/server.cpp \
  -o redisclone
```

## Run The TCP Server

```bash
./build/redisclone
```

Or, with the direct compiler output:

```bash
./redisclone
```

Connect from another terminal:

```bash
nc localhost 6379
```

Example session:

```text
SET name Aditya
OK
GET name
Aditya
EXPIRE name 60
1
INFO
Keys: 1
Clients: 1
Uptime: 12 sec
Memory: 184 bytes
```

## Console Mode

```bash
./build/redisclone --console
```

## Persistence

`SAVE` writes a snapshot to `data/dump.rdb` by default. The server loads this file at startup.

Use a custom path:

```bash
./build/redisclone --data /tmp/redisclone.rdb
```

## Tests

```bash
cmake -S . -B build
cmake --build build --target redisclone_tests
ctest --test-dir build
```

Direct compile fallback:

```bash
c++ -std=c++20 -pthread -Iinclude \
  tests/test_runner.cpp src/command_executor.cpp src/command_parser.cpp src/datastore.cpp src/server.cpp \
  -o redisclone_tests
./redisclone_tests
```

## Benchmark

```bash
./build/redisclone_benchmark 100000
```

The benchmark performs `N` writes and `N` reads, then reports operations per second.

## Docker

```bash
docker build -t redisclone .
docker run --rm -p 6379:6379 redisclone
```

## Resume Summary

- Built a multithreaded in-memory key-value database inspired by Redis using C++20.
- Implemented TCP networking with POSIX sockets, concurrent client handling with `std::thread`, and thread-safe storage with `std::mutex`.
- Added persistence, expiring keys, background cleanup, runtime statistics, tests, and a custom benchmark.

