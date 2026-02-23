# Redis Lite

A lightweight Redis clone written in C++. Supports multiple simultaneous clients using `epoll`, non-blocking I/O and an in memory key value store.

## How It Works

1. A TCP socket is created and bound to port 6379
2. `epoll` watches all connected clients for incoming data
3. When a client sends a command, it is buffered until a full line is received
4. The command is parsed and dispatched to `process_and_reply()`
5. The response is sent back in Redis protocol format (RESP)

## Features

- Multi-client support via `epoll`
- Non-blocking sockets
- In memory key value store
- [Commands](#supported-commands)

## Build

```bash
g++ main.cpp -o redisLite
```

## Run

```bash
./redisLite
```

The server listens on port **6379** by default

## Connect

Use `nc` to connect and send commands manually

```bash
nc localhost 6379
```

## Supported Commands

| Command | Syntax | Description |
|---|---|---|
| `PING` | `PING` | Returns `+PONG` if server is alive |
| `INFO` | `INFO` | Get total key ammounts and clients connected |
| `FLUSHALL` | `FLUSHALL` | Clear the entire database |
| `SET` | `SET [key] [value]` | Stores a value under the given key |
| `SET` with expiry | `SET [key] [value] EX [seconds]` | Stores a value that expires after X seconds |
| `GET` | `GET [key]` | Returns the value for the key |
| `DEL` | `DEL [key]` | Delete existing key |
| `LPUSH` | `LPUSH [key] [value]` | Inserts a value at the head of the list. Returns the list length |
| `RPUSH` | `RPUSH [key] [value]` | Insert value at the end of the list |
| `LRANGE` | `LRANGE [key] [start] [end]` | Returns elements from the list between start and end indices |
| `LTRIM` | `LTRIM [start_index] [end_index]` | Trim a list |
| `RENAME` | `RENAME [key_name] [new_key_name]` | Rename existing keys |


## Project Structure

```
main.cpp   — all server logic
```
