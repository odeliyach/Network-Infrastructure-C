# PCC Interview Prep Guide

## Elevator Pitch (30 seconds)
The Printable Characters Counter (PCC) is a TCP client/server pair that streams arbitrary files over a deterministic protocol defined in `instructions_network.txt`. The client sends a 32-bit length (network byte order), streams that many bytes, and receives a 32-bit printable-character count back. The server is an iterative, signal-aware TCP listener that tallies printable ASCII (32–126) per session, aggregates totals across sessions, and emits statistics atomically on `SIGINT`. It uses bounded buffers, strict return-value checking, and endian-safe serialization (`htonl`/`ntohl`) to remain correct across heterogeneous hosts.

## Deep-Dive Questions & Answers

1. **TCP/IP & Sockets — Why `AF_INET` + `SOCK_STREAM`, and what happens if the connection drops mid-transfer?**  
   We use IPv4 TCP streams to guarantee ordered, reliable delivery that matches the PCC framing (N length → payload → count). UDP would risk loss/reordering and force us to reimplement reliability. If a drop occurs (e.g., `ETIMEDOUT`, `ECONNRESET`, `EPIPE`), the server treats it as a TCP error, logs to `stderr`, aborts that session, and *does not* merge its partial histogram into `pcc_total`, preserving global correctness.

2. **Concurrency — How does the server handle multiple clients, and why this model?**  
   The server is intentionally iterative: `accept()` → handle one client fully → loop. This keeps signal handling simple (atomic with respect to in-flight client) as required by the spec. Alternatives like `select()`/`epoll()` or threading could increase throughput, but would complicate the guarantee that `SIGINT` snapshots include exactly the completed client’s stats while avoiding races on `pcc_total`.

3. **Data Integrity — Why is network byte order non-negotiable here?**  
   The length (N) and count (C) are serialized with `htonl`/`ntohl` to force big-endian wire format. Without this, a little-endian host would misinterpret the 32-bit fields, leading to truncated reads, oversized allocations, or wrong counts. Using network byte order makes the PCC protocol portable across architectures and aligns with the assignment’s explicit requirement.

4. **Error Handling — How do we handle partial reads/writes and `EINTR`?**  
   Both client and server wrap `read_exact`/`write_exact` loops that continue until the requested byte count is satisfied, retry on `EINTR`, and treat zero-length reads during payload as remote closure. This prevents short reads/writes from silently corrupting framing or counts. Not checking return values could leave length fields half-written, causing peers to block or mis-parse the stream.

5. **Signal Safety — How is `SIGINT` handled safely to print stats?**  
   `sigaction` installs a handler that sets a `sig_atomic_t` flag. The main loop checks the flag between clients; if set mid-request, it finishes the current client before breaking. Statistics are printed after the loop, ensuring `pcc_total` reflects only fully processed sessions and avoiding unsafe work inside the handler.

6. **Edge Cases — What are potential weak spots and how to improve them?**  
   - **Slowloris/pacing clients:** An attacker could drip bytes and occupy the only handler. Mitigate by adding timeouts (e.g., `SO_RCVTIMEO`) or moving to `select()`/`epoll()` with per-connection timers.  
   - **Large inputs:** We bound buffers to 1 KiB, but `N` could be large; adding chunk-level progress logging or capping `N` server-side can prevent long single-connection monopolization.  
   - **Backlog saturation:** With backlog 10 and iterative handling, bursts can drop connections. A worker pool or event loop would improve availability while still guarding `pcc_total` with synchronization.

7. **What happens if the client misreports N (sends fewer/more bytes)?**  
   If fewer bytes arrive, the server hits EOF before `N` bytes and aborts the session without updating totals. If the client tries to send more, the server only reads `N` bytes and then returns the count; excess bytes remain unread and the connection closes, preventing protocol poisoning.

## Key Terminology Cheat Sheet
- **Byte Stream (TCP), File Descriptor, System Calls (`socket`, `bind`, `listen`, `accept`, `connect`, `read`, `write`)**
- **Network Byte Order / Endianness (`htonl`, `ntohl`)**
- **Protocol Framing (length-prefix, bounded buffers)**
- **Printable ASCII (32–126) Histogram**
- **Graceful Shutdown vs. TCP Reset**
- **Signal Handling (`sigaction`, `sig_atomic_t`, `EINTR`)**
- **Partial Read/Write Mitigation, Backlog, `SO_REUSEADDR`**
- **Concurrency Models (Iterative vs. `select`/`epoll`, Worker Pool)**

Use these keywords to anchor answers, then explain the trade-offs (why iterative over threaded, why network byte order, why retry on `EINTR`) to show senior-level reasoning tied directly to the PCC protocol.
