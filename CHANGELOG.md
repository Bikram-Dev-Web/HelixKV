# Changelog - HelixKV

All notable changes to this project are documented in this file.

---

## [1.0.0] - 2026-07-02
HelixKV reaches feature completion for version 1.0! This release provides a fully compliant RESP in-memory storage engine featuring active/passive TTL eviction, thread-safe asynchronous AOF log compaction, binary point-in-time snapshotting, and a hybrid boot recovery mechanism.

### Added
* **Event-Driven Networking**: Integrated a single-threaded non-blocking network socket handler using the `select()` system multiplexer.
* **Dual-Mode Protocol Parser**: Support for both standard REdis Serialization Protocol (RESP) multi-bulk arrays and space-separated text commands (inline mode).
* **Append-Only File (AOF)**: Added write durability logging for database mutations (`SET`, `DEL`, `CLEAR`, `EXPIREAT`).
* **Asynchronous AOF Compaction**: Added a background worker thread to rewrite logs, buffering live incoming commands in memory and merging them atomically on finalization.
* **Snapshot Persistence (RDB-like)**: Designed a cross-platform binary snapshot format and implemented non-blocking `BGSAVE` and blocking `SAVE` commands.
* **Hybrid Recovery Engine**: Boot recovery checks for AOF files first, falling back to RDB binary snapshots if AOF is absent.
* **TTL & Keyspace Expirations**: Added active eviction checks on key read paths and passive scanner ticks running every 5 seconds.
* **Automated Unit & Multi-Threaded Stress Tests**: Comprehensive testing for concurrency safety, data durability, and protocol serialization.

---

## Known Limitations
1. **Memory Limits**: The data storage resides entirely in RAM. The database cannot scale to datasets larger than the machine's physical memory.
2. **Standard Select Constraints**: The networking engine uses `select()`, which has a default descriptor limit of 1024 concurrent client sockets.
3. **No Database Replication**: There is no built-in master-replica sync, sentinel monitoring, or clustering.

---

## Future Roadmap (v2.0 Plan)
* **Custom Collections**: Implement List, Hash, and Set types using a generic `Variant` value schema.
* **High-Performance Event Loop**: Migrate the networking layer from `select()` to `epoll` (Linux) / `IOCP` (Windows) to handle more than 1024 concurrent client connections.
* **LSM Storage Engine**: Build an SSTable write pipeline and a write-ahead log to move database storage to disk, enabling larger-than-RAM database instances.
