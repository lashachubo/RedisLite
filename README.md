# Redis++

A lightweight Redis compatible server written in C++. Supports multiple simultaneous clients using `epoll`, non-blocking I/O, and an in memory key value store with optional key expiration.

## Features

- Multi-client support via `epoll` (Linux)
- Non-blocking sockets
- In-memory key-value store
- Key expiration with `SET [key] [value] EX [seconds]`
- Responds to `PING`, `SET`, `GET` and `DEL` commands

## Build

```bash
g++ main.cpp -o redis
```

## Run

```bash
./redis_clone
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
| `PING` | `PING` | Returns `+PONG`. Used to check if server is alive |
| `SET` | `SET [key] [value]` | Stores a value under the given key |
| `SET` with expiry | `SET [key] [value] EX [seconds]` | Stores a value that expires after X seconds |
| `GET` | `GET [key]` | Returns the value for the key, or `$-1` if not found/expired |
| `DEL` | `DEL [key]` | Returns `:1` if deletion successful, `:0` if not found |

## How It Works

1. A TCP socket is created and bound to port 6379
2. `epoll` watches all connected clients for incoming data
3. When a client sends a command, it is buffered until a full line is received
4. The command is parsed and dispatched to `process_and_reply()`
5. The response is sent back in Redis protocol format (RESP)

## Project Structure

```
main.cpp   — all server logic
```
