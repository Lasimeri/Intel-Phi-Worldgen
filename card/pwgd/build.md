# build.sh

Builds `pwgd` into `card/pwgd/out/` twice:

- **`pwgd-card`**: `knc-cc -static` from the Intel Phi 7120A tree. The card has no dynamic loader.
- **`pwgd-host`**: the host's `cc`, for testing the protocol locally, e.g. `pwgd-host 25690 8` and `pwgcheck 127.0.0.1 25690 <capture>`.

It also builds `pwgcheck` (host). Both pwgd builds are made from `pwgd.c` and the shared `../lib/shape.c`, `../lib/cdma.c` and
`../lib/capfile.c`, with `-O2 -ffp-contract=off`.
