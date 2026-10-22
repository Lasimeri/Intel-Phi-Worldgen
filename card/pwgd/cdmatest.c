/* cdmatest.c: checks card-owned DMA channel CHAN against the shared host-memory region at OFFSET. See cdmatest.md.
 *   cdmatest CHAN OFFSET_MIB */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "cdma.h"

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: cdmatest CHAN OFFSET_MIB\n"); return 2; }
    int chan = atoi(argv[1]);
    uint64_t off = (uint64_t)atoi(argv[2]) << 20;
    const size_t span = 2u << 20;
    cdma d;
    if (cdma_open(&d, chan)) return 1;
    printf("channel %d open: region 0x%llx, %llu MiB; ring page 0x%llx\n", chan, (unsigned long long)d.region_addr,
           (unsigned long long)(d.region_size >> 20), (unsigned long long)d.page_phys);
    if (off + span > d.region_size) { fprintf(stderr, "offset past the region\n"); return 1; }

    int fd = open("/dev/phihost", O_RDWR | O_SYNC);
    volatile uint64_t *win = fd < 0 ? MAP_FAILED : mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)off);
    if (win == MAP_FAILED) { perror("/dev/phihost"); return 1; }
    uint64_t land_phys[1];
    uint64_t *land = huge_alloc(span, land_phys, 1);
    if (!land) { fprintf(stderr, "no huge page\n"); return 1; }

    /* host region -> card: a pattern written through the uncached map, pulled by the engine */
    uint64_t nonce = (uint64_t)time(NULL) * 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 8192; i++) win[i] = nonce ^ (uint64_t)i;
    uint64_t s = cdma_submit(&d, d.region_addr + off, land_phys[0], 65536, 0, 0);
    if (!s || cdma_wait(&d, s, 2.0)) { fprintf(stderr, "pull: no completion\n"); return 1; }
    int bad = 0;
    for (int i = 0; i < 8192; i++) bad += land[i] != (nonce ^ (uint64_t)i);
    printf("pull 64 KiB region -> card: %d of 8192 words differ\n", bad);

    /* card -> host region: a pattern in card memory pushed by the engine, with a status word into the region */
    for (int i = 0; i < 8192; i++) land[i] = ~nonce ^ ((uint64_t)i << 7);
    win[16384] = 0;
    s = cdma_submit(&d, land_phys[0], d.region_addr + off + 65536, 65536, d.region_addr + off + 131072, nonce);
    if (!s || cdma_wait(&d, s, 2.0)) { fprintf(stderr, "push: no completion\n"); return 1; }
    int bad2 = 0;
    for (int i = 0; i < 8192; i++) bad2 += win[8192 + i] != (~nonce ^ ((uint64_t)i << 7));
    printf("push 64 KiB card -> region: %d of 8192 words differ; host status word %s\n", bad2,
           win[16384] == nonce ? "written" : "MISSING");
    printf("host check: offset %llu holds ~nonce ^ (i << 7), nonce %016llx\n", (unsigned long long)(off + 65536),
           (unsigned long long)nonce);

    /* bandwidth: 128 copies of 512 KiB each way (a descriptor carries at most 16383 lines) */
    for (int dir = 0; dir < 2; dir++) {
        double t0 = now();
        uint64_t last = 0;
        for (int i = 0; i < 128; i++) {
            uint64_t src = dir ? land_phys[0] : d.region_addr + off;
            uint64_t dst = dir ? d.region_addr + off : land_phys[0];
            last = cdma_submit(&d, src, dst, 1u << 19, 0, 0);
        }
        if (!last || cdma_wait(&d, last, 5.0)) { fprintf(stderr, "bandwidth: no completion\n"); return 1; }
        double t = now() - t0;
        printf("%s: 64 MiB in %.4f s = %.1f MB/s\n", dir ? "card -> region" : "region -> card", t, 64.0 * 1048576 / t / 1e6);
    }
    cdma_close(&d);
    return bad || bad2 ? 1 : 0;
}
