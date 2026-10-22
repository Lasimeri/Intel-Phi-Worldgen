# build.sh

Builds `replay` twice into `card/replay/out/`:

- **`replay-host`**: the host's `cc`, so the SSE2 doubles are IEEE. It checks the port's logic without the card.
- **`replay-card`**: `knc-cc` from the Intel Phi 7120A tree (`$PHI_TREE`, default `~/Intel Phi 7120A`, through its `toolchain/env.sh`), linked `-static`. The card has no `/lib/ld-musl-x86_64.so.1`, so a dynamic binary fails there with "No such file or directory".

Both builds use `-O2 -ffp-contract=off`. The flag stops any multiply and add
from being fused, which Java never does.
