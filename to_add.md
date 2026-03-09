

# Commands to Add


## String Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `SETNX` | `SETNX key value` | `:1` or `:0` | Set only if key does not exist. Useful for locks. |
| `STRLEN` | `STRLEN key` | `:length` | Returns the length of the string stored at key. |
| `GETSET` | `GETSET key value` | `$old_value` | Sets a new value and returns the old one atomically. |

---

## Key Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `KEYS` | `KEYS pattern` | `*array` | List all keys matching a pattern. `*` = all keys. Slow on large DBs. |
| `TYPE` | `TYPE key` | `+string` or `+list` etc. | Returns the type of value stored at key. |
| `TTL` | `TTL key` | `:seconds` | How many seconds until the key expires. `-1` = no expiry, `-2` = doesn't exist. |
| `EXPIRE` | `EXPIRE key seconds` | `:1` or `:0` | Set an expiration on an existing key. |
| `RANDOMKEY` | `RANDOMKEY` | `$key` | Returns a random key from the database. |

---

## List Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `LLEN` | `LLEN key` | `:length` | Returns the length of a list. |
| `LINDEX` | `LINDEX key index` | `$value` | Get element at a specific index. |
| `LSET` | `LSET key index value` | `+OK` | Set element at a specific index. |
| `LPOP` | `LPOP key` | `$value` | Remove and return the first element. |
| `RPOP` | `RPOP key` | `$value` | Remove and return the last element. |
| `LINSERT` | `LINSERT key BEFORE\|AFTER pivot value` | `:new_length` | Insert a value before or after another value in the list. |

---

## Hash Commands (new data type)

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `HMSET` | `HMSET key f1 v1 f2 v2` | `+OK` | Set multiple fields at once. |
| `HMGET` | `HMGET key f1 f2` | `*array` | Get multiple fields at once. |
| `HGETALL` | `HGETALL key` | `*array` | Get all fields and values. |
| `HDEL` | `HDEL key field` | `:1` or `:0` | Delete a field from a hash. |
| `HEXISTS` | `HEXISTS key field` | `:1` or `:0` | Check if a field exists. |
| `HLEN` | `HLEN key` | `:count` | Number of fields in the hash. |
| `HKEYS` | `HKEYS key` | `*array` | All field names. |
| `HVALS` | `HVALS key` | `*array` | All field values. |

---

## Set Commands (new data type)

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `SADD` | `SADD key value` | `:1` or `:0` | Add a value to the set. Returns 0 if already existed. |
| `SREM` | `SREM key value` | `:1` or `:0` | Remove a value from the set. |
| `SMEMBERS` | `SMEMBERS key` | `*array` | Return all members of the set. |
| `SISMEMBER` | `SISMEMBER key value` | `:1` or `:0` | Check if a value is in the set. |
| `SCARD` | `SCARD key` | `:count` | Number of members in the set. |

---

## Server Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `DBSIZE` | `DBSIZE` | `:count` | Number of keys in the database. Simpler than `INFO`. |
| `SAVE` | `SAVE` | `+OK` | Manually trigger a save to disk. You already have `save_database()`. |
| `SELECT` | `SELECT index` | `+OK` | Switch between numbered databases (0-15). Requires multiple `g_database` maps. |
| `ECHO` | `ECHO message` | `$message` | Returns the message back. Useful for testing. |
| `COMMAND` | `COMMAND` | list of commands | Returns info about available commands. Useful for redis-cli compatibility. |
