

# Commands to Add


## String Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `SETNX` | `SETNX key value` | `:1` or `:0` | Set only if key does not exist. Useful for locks. |
| `GETSET` | `GETSET key value` | `$old_value` | Sets a new value and returns the old one atomically. |

---

## Key Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `KEYS` | `KEYS pattern` | `*array` | List all keys matching a pattern. `*` = all keys. Slow on large DBs. |
| `TTL` | `TTL key` | `:seconds` | How many seconds until the key expires. `-1` = no expiry, `-2` = doesn't exist. |
| `RANDOMKEY` | `RANDOMKEY` | `$key` | Returns a random key from the database. |

---

## List Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `LSET` | `LSET key index value` | `+OK` | Set element at a specific index. |
| `LINSERT` | `LINSERT key BEFORE\|AFTER pivot value` | `:new_length` | Insert a value before or after another value in the list. |

---

## Hash Commands (new data type)

| Command | Syntax | Returns | Notes |
|---|---|---|---|

---

## Set Commands (new data type)

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `SREM` | `SREM key value` | `:1` or `:0` | Remove a value from the set. |
| `SMEMBERS` | `SMEMBERS key` | `*array` | Return all members of the set. |
| `SISMEMBER` | `SISMEMBER key value` | `:1` or `:0` | Check if a value is in the set. |
| `SCARD` | `SCARD key` | `:count` | Number of members in the set. |

---

## Server Commands

| Command | Syntax | Returns | Notes |
|---|---|---|---|
| `SELECT` | `SELECT index` | `+OK` | Switch between numbered databases (0-15). Requires multiple `g_database` maps. |
