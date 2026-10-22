# pwgcheck.c

```
pwgcheck HOST PORT CAPTURE_DIR [ROUNDS]
```

A host tool that drives a running `pwgd` the way the mod does, with a
capture's own inputs, and checks every block it gets back against the
capture's `SHAP`:

1. Sends `world.bin` as the hello.
2. Times one urgent request on its own.
3. Sends every captured chunk ROUNDS times as prefetches and decodes each answer's column runs.

It prints the round trip, chunks per second, bytes per chunk and how many
chunks differ, and exits 1 when any do. Built by hand next to the daemon:

```
cc -O2 -std=gnu11 -I../lib -o out/pwgcheck pwgcheck.c ../lib/capfile.c ../lib/shape.c
./out/pwgcheck 127.0.0.1 25600 ~/.cache/phiworldgen/capture/s2a 5
```
