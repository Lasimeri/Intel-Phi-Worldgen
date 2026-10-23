#!/usr/bin/env bash
# build.sh: pwgd for the card as out/pwgd-card-new (deploying copies it to out/pwgd-card, which the keeper puts on a
# card that lost pwgd; knc-cc, x87, static; the 3D octaves on the vector unit, compiled by the
# Intel-Phi-LLVM S3 clang) and the host tools (pwgcheck, shmcheck). See build.md.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
lib="$here/../lib"
out="$here/out"
mkdir -p "$out"
flags=(-O2 -std=gnu11 -Wall -Wextra -ffp-contract=off -I"$lib")
cc "${flags[@]}" -o "$out/pwgcheck" "$here/pwgcheck.c" "$lib/shape.c" "$lib/capfile.c"
cc "${flags[@]}" -o "$out/shmcheck" "$here/shmcheck.c" "$lib/shape.c" "$lib/capfile.c" "$lib/genlayer.c" "$lib/surface.c" \
    "$lib/carve.c"
VCC="${PHI_LLVM_BIN:-$HOME/.cache/intel-phi-llvm-build/llvm/bin}/clang"
SYS="${PHI_SYSROOT_7120A:-$HOME/.cache/intel-phi-7120a-build/toolchain/sysroot}"
"$VCC" --target=x86_64-unknown-linux-musl -march=knc --sysroot="$SYS" "${flags[@]}" -fno-jump-tables \
    -falign-loops=1 -fPIE -c -o "$out/vshape.o" "$lib/vshape.c"
"$VCC" --target=x86_64-unknown-linux-musl -march=knc -fPIE -c -o "$out/kncgather.o" "$lib/kncgather.S"
# shellcheck disable=SC1091
. "${PHI_TREE:-$HOME/Intel Phi 7120A}/toolchain/env.sh"
knc-cc "${flags[@]}" -static -o "$out/pwgd-card-new" "$here/pwgd.c" "$lib/shape.c" "$lib/capfile.c" "$lib/cdma.c" "$lib/genlayer.c" "$lib/rwg.c" \
    "$lib/surface.c" "$lib/carve.c" \
    "$out/vshape.o" "$out/kncgather.o" -lpthread
"${PHI_TREE:-$HOME/Intel Phi 7120A}/host/target/debug/phi-isa-audit" "$out/pwgd-card-new" | tail -1
ls -l "$out"
