# Redis Lite

A lightweight Redis clone written in C++. Supports multiple simultaneous clients using `epoll`, nonblocking I/O and an in memory key value store.

## How It Works

1. A TCP socket is created and bound to port 6379
2. `epoll` watches all connected clients for incoming data
3. When a client sends a command, it is buffered until a full line is received
4. The command is parsed and dispatched to `process_and_reply()`
5. The response is sent back in Redis protocol format (RESP)

## Features

- Multi client support via `epoll`
- Nonblocking sockets
- In memory key value store
- [Commands](#supported-commands)

## Build

```bash
make
```

## Run

```bash
./server
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
| `HELP` | `HELP` | List all available commands |
| `SET` | `SET [key] [value]` | Stores a value under the given key |
| `SET` with expiry | `SET [key] [value] EX [seconds]` | Stores a value that expires after X seconds |
| `EX` | `EX [key] [seconds]` | Add expiration to a key |
| `PERSIST` | `PERSIST [key]` | Remove the expiration from a key |
| `RENAME` | `RENAME [key] [new_key]` | Rename existing keys |
| `STRLEN` | `STRLEN [key]` | Returns the lenght of the string |
| `MSET` | `MSET [key1] [value1] [key2] [value2]` | Set multiple key value pairs |
| `GET` | `GET [key]` | Returns the value for the key |
| `MGET` | `MGET [key1] [key2]` | Get values of multiple keys |
| `DEL` | `DEL [key]` | Delete existing key |
| `LPUSH` | `LPUSH [key] [value]` | Inserts a value at the head of the list. Returns the list length |
| `RPUSH` | `RPUSH [key] [value]` | Insert value at the end of the list |
| `LPOP` | `LPOP [key]` | Remove and return the first element |
| `RPOP` | `RPOP [key]` | Remove and return the last element |
| `LRANGE` | `LRANGE [key] [start] [end]` | Returns elements from the list between start and end indices |
| `LINDEX` | `LINDEX [key] [index]` | Get element at a specific index |
| `LTRIM` | `LTRIM [start_index] [end_index]` | Trim a list |
| `LLEN` | `LLEN [key]` | Get the length of a list |
| `INCR` | `INCR [key]` | Increase value by 1 |
| `INCRBY` | `INCRBY [key] [ammount]` | Increase value by x |
| `DECR` | `DECR [key]` | Decrease value by 1 |
| `DECRBY` | `DECRBY [key] [ammount]` | Decrease value by x |
| `APPEND` | `APPEND [key] [value]` | Append a value to a key |
| `HSET` | `HSET [key] [field] [value]` | Set a key value pair in hash |
| `HGET` | `HGET [key] [field]` | Get a value from hash |
| `EXISTS` | `EXISTS [key]` | Check if key exists |
| `PING` | `PING` | Returns `+PONG` if server is alive |
| `INFO` | `INFO` | Get total key ammounts and clients connected |
| `TYPE` | `TYPE [key]` | Return the type of value stored at key |
| `ECHO` | `ECHO [message]` | Returns a message back |
| `DBSIZE` | `DBSIZE` | Returns the number of keys in the database |
| `FLUSHALL` | `FLUSHALL` | Clear the entire database |

## Project Structure

```
entry.h      — data types for keys
commands.h   — extern globals + function prototypes
commands.cpp — all commands
main.cpp     — server setup and event loop
Makefile     — build
```