# vpucount.c

Counts the card's vector unit at work with the core's performance counters:
how many VPU instructions a process or a command executed in user mode. It is
the hardware proof that a build uses the VPU, beside phitop's core grid (phitop's
`vpu` line is the AVX-512 wrapper worker's own statistics, not a counter).

    vpucount [-e CODE,...] -p PID -t SECONDS     every thread of a running process
    vpucount [-e CODE,...] -- COMMAND [ARGS]     a command, its threads included

- `-e`: KNC event codes, event in bits 0 to 7 and unit mask in bits 8 to 15 (the
  card's `cpu` PMU, `/sys/bus/event_source/devices/cpu/format`). The default is
  `0x2016`, VPU_INSTRUCTIONS_EXECUTED. Others: `0x2018` VPU_ELEMENTS_ACTIVE,
  `0x0016` INSTRUCTIONS_EXECUTED, `0x002a` CPU_CLK_UNHALTED (codes from libpfm4,
  `lib/events/intel_knc_events.h`).
- With `-p`, SIGTERM or SIGINT ends the window early and the counts are printed.
- Counts are scaled by time enabled over time running, which differ only if the
  kernel multiplexed the counters.

On the 7120A (kernel 7.2.3, 2026-10-22) only ONE event counts per run: a second
event reads 0, in a group or on its own fd. Count one event a run.

Measured (card 0; logs in `~/.cache/phiworldgen/`):

| What | VPU_INSTRUCTIONS_EXECUTED |
| --- | --- |
| replay s2a, verification only (91 scalar shapes + 91 vector octaves) | 104,668,096 |
| the same plus 455 vector-octave chunks | 628,008,576 (about 1.15 M a chunk) |
| replay with 91 scalar timing chunks added | 104,668,096 (the scalar x87 path adds 0) |
| pwgd idle, 122 threads, 5.18 s | 0 |

Build: `knc-cc -O2 -std=gnu11 -Wall -Wextra -static -o vpucount vpucount.c`
(`. "$HOME/Intel Phi 7120A/toolchain/env.sh"` first, from bash).
