# HelixKV v1.0 Developer Guide

This guide is designed for future contributors and interview preparation. It documents the core modules, wire protocols, persistence formats, and critical design decisions of the HelixKV engine.

---

## 1. Network Engine (`Server`)
HelixKV implements a single-threaded network event loop to avoid multithreading complexity, race conditions on network channels, and connection thread-scaling limits.

* **Event Loop Multiplexing**: Built around the standard socket interface `select()`. The loop monitors the listening socket for incoming connections and active client sockets for readability.
* **Stream Session Buffers**: To handle network fragmentation (partial reads/writes), the server allocates a session buffer string inside `client_buffers_` for each socket descriptor. Command execution only occurs once a complete line delimiter (`\n`) or full RESP array byte count is present.
* **Non-Blocking Write Loop**: Responses are sent through a custom non-blocking write loop `sendResponse()`. If a socket output buffer is full (`WSAEWOULDBLOCK` or `EWOULDBLOCK`), the loop yields to the OS schedulers to avoid hogging the CPU before attempting to write remaining bytes.

---

## 2. Storage Engine (`Storage`)
The core database state is kept in memory to deliver fast access speeds:

* **Primary Map**: `data_` is a `std::unordered_map<std::string, std::string>` containing key-value pairs.
* **Expirations Map**: `expirations_` is a `std::unordered_map<std::string, std::uint64_t>` mapping keys to absolute Unix Epoch timestamps.
* **Active Eviction**: Every read path (`GET`, `EXISTS`, `KEYS`, `SIZE`) first executes `checkAndEvict()`. If a key has expired, it is deleted from both memory maps, a `DEL` command is written to the persistence file, and the lookup returns a database miss.
* **Passive Eviction**: A steady-clock check executes every 5 seconds inside the server event loop. It scans the `expirations_` map, passively deletes expired keys, and writes corresponding `DEL` records to the durability logs.

---

## 3. Wire Protocol Implementation (`RESP Parser`)
HelixKV supports **dual-protocol parsing** to remain compatible with telnet clients and standard Redis clients:

1. **RESP Protocol**: If the client buffer starts with `*`, it is routed to the multi-bulk parser.
   * Format: `*<count>\r\n$<length>\r\n<element>\r\n...`
   * Safely parses parameters with spaces since each argument is framed with an explicit size indicator.
2. **Inline Mode**: If the buffer starts with normal text characters, it splits input by space characters and terminates on newlines (`\n`).

### Response Formatting Table
Based on the incoming protocol format, response serializers dynamically shape replies:

| Command | RESP Client Formatting | Inline Text Client Formatting |
| :--- | :--- | :--- |
| **PING** | `+PONG\r\n` | `PONG\n` |
| **SET** | `+OK\r\n` | `OK\n` |
| **GET** | `$<len>\r\n<value>\r\n` or `$-1\r\n` | `<value>\n` or `NULL\n` |
| **DEL** | `:1\r\n` (existed) or `:0\r\n` | `DELETED\n` or `\n` |
| **EXISTS** | `:1\r\n` or `:0\r\n` | `1\n` or `0\n` |
| **KEYS** | `*<count>\r\n$<len>\r\n<k>\r\n` | `<key1> <key2>\n` |
| **SIZE** | `:<size>\r\n` | `<size>\n` |
| **CLEAR** | `+OK\r\n` | `OK\n` |
| **EXPIRE** | `:1\r\n` or `:0\r\n` | `1\n` or `0\n` |
| **TTL** | `:<seconds>\r\n` (or `-1` / `-2`) | `<seconds>\n` |
| **BGSAVE** | `+Background saving started\r\n` | `Background saving started\n` |

---

## 4. Persistence Architecture
HelixKV implements a hybrid persistence strategy designed for time-durable safety:

### Append Only File (AOF)
* **Write Logging**: Mutating commands are formatted as text commands (`SET key val`, `DEL key`, `EXPIREAT key epoch`) and appended to the disk file immediately.
* **Asynchronous Compaction (`BGREWRITEAOF`)**: A background worker thread dumps the in-memory maps to a temporary file. Edits that arrive while the worker is writing are captured in `rewrite_buffer_` and merged atomically during finalization.
* **Exclusivity Guard**: Background tasks (`BGSAVE` and `BGREWRITEAOF`) check flags to prevent concurrent executions, saving system I/O resources.

### Snapshotting (RDB)
* **Binary Format Schema**: Saves database state into a compact binary layout:
```text
+-----------------------+---------------------+-------------------+
|  MAGIC (8B)           | VERSION (4B)        | ENTRY COUNT (8B)  |
|  "HELIXRDB"           | "0001"              | uint64_t          |
+-----------------------+---------------------+-------------------+
|  HAS_EXPIRE (1B)      | EXPIRE_TIME (8B)    | KEY_LEN (4B)      |
|  0 or 1               | (only if has_exp=1) | uint32_t          |
+-----------------------+---------------------+-------------------+
|  KEY_DATA (var)       | VALUE_LEN (4B)      | VALUE_DATA (var)  |
|  raw chars            | uint32_t            | raw chars         |
+-----------------------+---------------------+-------------------+
```

---

## 5. Critical Design Decisions
1. **Time Consistency across Server Reboots**: We log `EXPIREAT key epoch_seconds` (absolute timestamp) rather than relative offsets (`EXPIRE key offset_seconds`). This prevents keys from resetting their expiration durations when the server restarts.
2. **Recursive Mutex Deadlock Resolution**: The storage engine uses standard lock guards. Previously, triggering compaction directly inside a setter attempted to re-acquire the mutex, deadlocking the server. This was resolved by implementing `rewriteAOF_Lockless()`.
3. **Windows File Access Swapping**: Windows filesystem locks open streams. Compaction and RDB finalization handles file swaps cleanly by explicitly closing input/output streams, invoking `std::remove()`, and renaming the temp files using `std::rename()`.
