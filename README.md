# HelixKV v1.0

HelixKV is a high-performance, Redis-inspired, in-memory key-value store built from the ground up in **C++20**. It features an event-driven network architecture, dual-protocol parsers (supporting `redis-cli`), hybrid persistence recovery (AOF log compaction + binary RDB snapshots), and active/passive TTL evictions.

Designed for systems programming showcases and high-throughput cache environments, HelixKV delivers sub-millisecond responses over a single-threaded multiplexed event loop.

---

## Key Features

* **Event-Driven Networking**: Client connections multiplexed concurrently over a single thread using non-blocking socket I/O and the `select()` system call.
* **Dual-Protocol Compatibility**: Dynamically parses standard **RESP (Redis Serialization Protocol)** commands alongside raw inline text. Directly interfaces with standard toolchains like `redis-cli`.
* **Hybrid Durability**:
  * **Append-Only File (AOF)**: Transactional logs recorded for data mutations.
  * **Non-Blocking Compaction (`BGREWRITEAOF`)**: A background worker thread compacts log size asynchronously, double-buffering live database writes to prevent blocking.
  * **Snapshotting (`SAVE` / `BGSAVE`)**: Dumps point-in-time database states to a custom, compact binary format on disk.
* **Active & Passive TTL Evictions**: Key cache lifetimes are enforced actively upon client reads and passively through a background clock tick running every 5 seconds.
* **Durability Across Reboots**: Computes absolute system epoch timestamps to guarantee correct cache lifespans across database restarts.

---

## Directory Layout

```text
HelixKV/
├── docs/                     # Architectural & Developer guides
│   ├── architecture.md       # Sequence & component diagrams
│   └── developer_guide.md    # Low-level systems implementation details
├── src/                      # Source Code
│   ├── command_handler.cpp   # Routing & RESP protocol serialization
│   ├── persistence.cpp       # Binary RDB and text AOF serialization
│   ├── server.cpp            # Socket event loop & buffer assemblies
│   └── storage.cpp           # In-memory maps & eviction threads
├── tests/                    # Tests
│   ├── test_main.cpp         # Unit & integration assertions
│   └── stress_test.cpp       # Concurrency benchmark tool
├── CHANGELOG.md              # v1.0.0 release notes & roadmap
└── CMakeLists.txt            # Project build configurations
```

---

## Quickstart

### Prerequisites
* A C++20 compatible compiler (GCC 11+, Clang 13+, or MSVC 2022+)
* CMake (Version 3.15+)
* Ninja or Make build tool

### Build Instructions
```bash
# Clone the repository
git clone https://github.com/Bikram-Dev-Web/HelixKV.git
cd HelixKV

# Configure CMake
cmake -B build -S .

# Build binary
cmake --build build
```

### Running the Server
```bash
# Start HelixKV Server (default port: 6379)
./build/helixkv.exe
```

### Connecting with `redis-cli`
Since HelixKV is fully RESP-compatible, you can connect directly using standard Redis CLI utilities:
```bash
redis-cli -p 6379
```

---

## Example Usage

```text
127.0.0.1:6379> SET user_session token_abc123
OK
127.0.0.1:6379> GET user_session
"token_abc123"
127.0.0.1:6379> EXISTS user_session
(integer) 1
127.0.0.1:6379> EXPIRE user_session 10
(integer) 1
127.0.0.1:6379> TTL user_session
(integer) 8
127.0.0.1:6379> BGSAVE
"Background saving started"
127.0.0.1:6379> BGREWRITEAOF
"Background rewrite of AOF started"
```

---

## Performance & Concurrency Benchmarks
HelixKV has been stress-tested under high concurrent load simulating production environments:

* **Hardware Environment**: Intel Core i7-10750H @ 2.60GHz, 16GB RAM.
* **Concurrent Loads**: 100 client connections writing 100,000 operations.
* **Observed Throughput**: Sub-millisecond execution times per write payload with **zero errors, socket timeouts, or data corruption**.
* **Compaction Stability**: 10 background auto-compaction triggers completed successfully *during* active stress query loads without blocking client reads.

---

## Technical Specifications
For low-level implementation details, see:
* [Architecture Specifications](docs/architecture.md): Sequence diagrams illustrating the event loop, storage locking, and hybrid boot recovery.
* [Developer Reference Guide](docs/developer_guide.md): Details on the RESP parser, active/passive eviction hooks, and cross-platform file swapping workarounds.