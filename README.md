<div align="center">

# Printable Characters Counter (PCC)

<a href="https://github.com/odeliyach/Network-Infrastructure-C/actions"><img src="https://github.com/odeliyach/Network-Infrastructure-C/actions/workflows/ci.yml/badge.svg"></a>
<a href="https://github.com/odeliyach/Network-Infrastructure-C/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg"></a>
</div>

## 🚀 Read This First
- The authoritative project compass is [`docs/instructions_network.txt`](docs/instructions_network.txt). All development choices, error semantics, and protocol details align with that document.

## 🧠 Architecture Overview
- **Role:** Implements a TCP printable-characters counter with deterministic client/server protocol flow.
- **Server (`pcc_server`)**: Accepts TCP sessions, counts printable bytes (ASCII 32–126) from each byte stream, persists aggregated frequency metrics, and emits statistics on atomic SIGINT handling.
- **Client (`pcc_client`)**: Streams a file over TCP using the mandated protocol, prints the printable-count returned by the server, and exits with zero on success.
- **Concurrency Model:** Iterative single-threaded loop with signal-aware acceptance; SIGINT processing is atomic relative to in-flight client handling as required by the assignment text.

### 🧠 Protocol Diagram
```mermaid
sequenceDiagram
    participant Client
    participant Server
    Client->>Server: N (uint32, network byte order)
    Server-->>Client: Length accepted
    Client->>Server: N bytes payload
    Server->>Server: Count printable bytes
    Server-->>Client: C (uint32, network byte order)
```

## 🧠 Core Networking Concepts
- **TCP Handshake:** The server binds to `INADDR_ANY`, listens with backlog `10`, and completes the three-way handshake before payload exchange. This guarantees reliable, ordered byte streams prior to protocol framing.
- **Network Byte Order (Endianness):** Length and count fields are serialized with `htonl`/`ntohl` to enforce big-endian wire representation across heterogeneous hosts.
- **PCC Algorithm:** Byte streams are ingested in bounded buffers (1 KiB) to avoid oversized allocations. Each byte is classified as printable or non-printable; printable bytes increment both per-connection and global frequency vectors. Only successful sessions contribute to `pcc_total`, ensuring TCP errors or premature disconnects do not contaminate statistics.
- **Robust Error Handling:** All critical system calls (`socket`, `bind`, `listen`, `accept`, `read`, `write`, `connect`) are validated. TCP-specific disconnect conditions (`ETIMEDOUT`, `ECONNRESET`, `EPIPE`) log and continue without mutating global counters; other failures are fatal with descriptive diagnostics.

## 🛠️ Build, Test, and Run
- Toolchain: `gcc`, `make` (flags: `-Wall -Werror -O3 -std=c11 -D_POSIX_C_SOURCE=200809`).
- Build artifacts: `bin/pcc_server`, `bin/pcc_client`.
- Commands:
  - `make all` — compile server and client.
  - `make test` — loopback integration smoke test using a temporary file and SIGINT-driven shutdown.
  - `make clean` — remove build outputs.
- Manual execution examples:
  - `./bin/pcc_server 5555`
  - `./bin/pcc_client 127.0.0.1 5555 ./path/to/file`

## 🐳 Dockerized Execution
1. Build: `docker build -t pcc .`
2. Run server: `docker run --rm -p 5555:5555 pcc ./bin/pcc_server 5555`
3. Run client (loopback to host or container IP): `docker run --rm --network host -v $(pwd):/data pcc ./bin/pcc_client 127.0.0.1 5555 /data/yourfile`

## 🔄 CI/CD
- GitHub Actions workflow `ci.yml` performs `make all` and `make test` on every push and pull request to enforce compilation hygiene and protocol smoke coverage.

## 🧭 Repository Layout
- `src/` - C sources (`pcc_server.c`, `pcc_client.c`).
- `docs/` - assignment and protocol instructions (`instructions_network.txt`).
- `bin/` - build outputs (generated).

## Academic Integrity Notice

**This repository is for portfolio and educational purposes only.** If you are a student currently taking an Operating Systems course, copying this code violates academic integrity policies. I do not take responsibility for disciplinary actions against individuals who misuse this code.


## 📄 License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

---
<div align="center">

Built by **Odeliya Charitonova** · [GitHub](https://github.com/odeliyach) · [LinkedIn](https://linkedin.com/in/odeliya-charitonova)

*Computer Science student @ Tel Aviv University, School of CS & AI*

</div>

