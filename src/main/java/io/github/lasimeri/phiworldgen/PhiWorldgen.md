# PhiWorldgen.java

The Forge `@Mod` entry point.

- **preInit:** reads `config/phiworldgen.cfg`, category `card`:

  | Key | Default | What it does |
  | --- | --- | --- |
  | `enabled` | true | use the card for the terrain shape |
  | `host` | 127.0.0.1 | where pwgd is reached: the `ssh -L` forward |
  | `port` | 25600 | pwgd's port |
  | `prefetchRadius` | 4 | how far around each chunk made to prefetch |
  | `prefetchPerChunk` | 16 | at most this many prefetch requests per chunk made |
  | `waitMs` | 1 | at most how long the server thread waits for an answer already asked for |
  | `transport` | auto | `shm`: the shared host-memory region (`docs/shm-protocol.md`); `tcp`: the ssh forward; `auto`: the region, else TCP |
  | `shmFile` | `/dev/shm/phi-hostmem` | the host side of the region |
  | `shmOffsetMiB` | 1024 | where the mod's part of the region starts |
  | `check` | false | compute every chunk on both and compare |

- **serverStarting:** registers `/phiwg` (`command/CommandPhiwg`).
- **serverStarted:** logs `GenProfile`'s report for the spawn area and resets it. It then starts `phiwg-card-connect`, which connects to pwgd (`card/CardClient`) whenever there is no connection, every 10 s. It stops for good when the overworld generator is not exactly vanilla's (`Capture.worldBlob` refuses it).
- **serverStopping:** closes the connection.

The generator itself is reached through mixins (`mixins/`, listed in
`src/main/resources/mixins.phiworldgen.json`).

`acceptableRemoteVersions = "*"` does not let vanilla clients in: Forge
1.7.10 kicks any client without FML (`mixins/MixinVanillaTimeoutWaiter.md`).
A client needs Forge 1.7.10, but not this mod.
