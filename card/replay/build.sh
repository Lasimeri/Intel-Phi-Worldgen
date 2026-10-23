#!/usr/bin/env bash
# build.sh: the replay for the host (cc, SSE2) and for the card (knc-cc, x87, static; the 3D octaves also on the
# vector unit, compiled by the Intel-Phi-LLVM S3 clang). See build.md.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
lib="$here/../lib"
out="$here/out"
mkdir -p "$out"
src=("$here/replay.c" "$lib/shape.c" "$lib/capfile.c" "$lib/genlayer.c" "$lib/surface.c" "$lib/carve.c")
flags=(-O2 -std=gnu11 -Wall -Wextra -ffp-contract=off -I"$lib")
cc "${flags[@]}" -o "$out/replay-host" "${src[@]}" -lpthread
# the RWG height stage's replay (rwgreplay.md)
rwgsrc=("$here/rwgreplay.c" "$lib/rwg.c" "$lib/capfile.c")
cc "${flags[@]}" -o "$out/rwgreplay-host" "${rwgsrc[@]}" -lpthread -lm
# the vector kernel: the series clang, -march=knc, against the 7120A tree's musl headers
VCC="${PHI_LLVM_BIN:-$HOME/.cache/intel-phi-llvm-build/llvm/bin}/clang"
SYS="${PHI_SYSROOT_7120A:-$HOME/.cache/intel-phi-7120a-build/toolchain/sysroot}"
"$VCC" --target=x86_64-unknown-linux-musl -march=knc --sysroot="$SYS" "${flags[@]}" -fno-jump-tables \
    -falign-loops=1 -fPIE -c -o "$out/vshape.o" "$lib/vshape.c"
"$VCC" --target=x86_64-unknown-linux-musl -march=knc -fPIE -c -o "$out/kncgather.o" "$lib/kncgather.S"
# shellcheck disable=SC1091
. "${PHI_TREE:-$HOME/Intel Phi 7120A}/toolchain/env.sh"
knc-cc "${flags[@]}" -DREPLAY_VPU -static -o "$out/replay-card" "${src[@]}" "$out/vshape.o" "$out/kncgather.o" -lpthread
knc-cc "${flags[@]}" -static -o "$out/rwgreplay-card" "${rwgsrc[@]}" -lpthread -lm
ls -l "$out"
