# capfile.h, capfile.c

Reads a capture file (`docs/capture-format.md`) whole into memory and finds
its sections by tag. `cap_load` checks the magic (`PWGW` for `world.bin`,
`PWGC` for a chunk) and the version; `cap_section` walks the sections from
byte 8, each a tag, a `u32` length and the payload; `cap_need` is the same
but exits with the tag and the sizes when a section is missing or short.

The `rd*` helpers read little-endian values through `memcpy`, which the
compiler turns into plain loads: the card (Knights Corner runs x86-64) and
the host are both little-endian, so no byte swapping is needed. They avoid
unaligned-pointer casts, since the payloads start wherever the previous
section ended.
