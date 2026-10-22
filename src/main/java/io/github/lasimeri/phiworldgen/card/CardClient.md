# CardClient.java, ShmLink.java, TcpLink.java

The mod's side of the card's pwgd.

**`CardClient`** holds what does not depend on the transport:
- the pending requests by chunk key, each a `CompletableFuture` for its column runs;
- the counters;
- the generator whose state the card was given (`provider`; the shape mixin serves only that one);
- the link in use.

It also contains:
- **`connect(transport, ...)`:** tries the shared region (`"auto"`, `"shm"`), else TCP (`"auto"`, `"tcp"`).
- **`request`:** sends a request unless one is pending; it returns null when the link has no room, and the CPU then makes that chunk.
- **`deliver`:** called by the link's reader thread.
- **`fail`:** called on an I/O error or by `close`; it fails every pending entry, unregisters the client and closes the link. `PhiWorldgen` reconnects every 10 s.
- **The constructor:** creates and completes one `Entry`, so that `Entry` and `CompletableFuture` are loaded and initialised on the connecting thread. Their first use cost the server thread 137 ms (measured 2026-10-22: the first chunk after a connect).

**`ShmLink`**, the shared host-memory region (`docs/shm-protocol.md`):
- **Connect:** maps the mod's part of `/dev/shm/phi-hostmem` with `FileChannel.map`, then:
  1. checks pwgd's heartbeat;
  2. clears the completion ring and the counters;
  3. writes the world state;
  4. raises the session after a store fence;
  5. waits for pwgd to echo it.
- **`send`:** writes a 256-byte entry; there is no room when 4096 are unconsumed or 2048 unanswered. **`flush`** publishes posted after a store fence.
- **The reader thread** waits for the next completion entry. It reads the slot after a load fence, advances completed, and fails the client when pwgd's heartbeat stops for 2 s or its session moves.
- **Fences:** `sun.misc.Unsafe`'s `storeFence` and `loadFence`, reached through method handles, so the source compiles for Java 8 without the class and the calls inline.

**`TcpLink`**: the socket protocol of `card/pwgd/pwgd.md`, through the ssh forward.
