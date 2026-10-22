/* pwgcheck.c: drives a running pwgd with a capture and checks every block it returns. See pwgcheck.md.
 *   pwgcheck HOST PORT CAPTURE_DIR [ROUNDS] */
#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "capfile.h"
#include "shape.h"

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static int read_full(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return -1;
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

typedef struct {
    int cx, cz;
    uint16_t bi10[100];
    uint16_t *want;
} chunk;

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: pwgcheck HOST PORT CAPTURE_DIR [ROUNDS]\n"); return 2; }
    int rounds = argc > 4 ? atoi(argv[4]) : 1;
    char path[4096];
    snprintf(path, sizeof path, "%s/world.bin", argv[3]);
    capfile wf;
    if (cap_load(&wf, path, "PWGW")) return 1;

    chunk *cs = NULL;
    int n = 0, cap = 0;
    DIR *d = opendir(argv[3]);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (strncmp(e->d_name, "c_", 2) != 0) continue;
        snprintf(path, sizeof path, "%s/%s", argv[3], e->d_name);
        capfile cf;
        if (cap_load(&cf, path, "PWGC")) return 1;
        if (n == cap) cs = realloc(cs, sizeof(chunk) * (size_t)(cap = cap ? cap * 2 : 64));
        const uint8_t *pos = cap_need(&cf, path, "CHNK", 8), *bi = cap_need(&cf, path, "BI10", 200);
        const uint8_t *sh = cap_need(&cf, path, "SHAP", 131072);
        cs[n].cx = rd32(pos);
        cs[n].cz = rd32(pos + 4);
        for (int i = 0; i < 100; i++) cs[n].bi10[i] = rd16(bi + 2 * i);
        cs[n].want = malloc(131072);
        for (int i = 0; i < 65536; i++) cs[n].want[i] = rd16(sh + 2 * i);
        n++;
        cap_free(&cf);
    }
    if (d) closedir(d);

    int fd = socket(AF_INET, SOCK_STREAM, 0), on = 1;
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((uint16_t)atoi(argv[2])) };
    inet_pton(AF_INET, argv[1], &a.sin_addr);
    if (connect(fd, (struct sockaddr *)&a, sizeof a)) { perror("connect"); return 1; }
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
    double t0 = now();
    uint8_t hdr[12];
    uint32_t one = 2, len = (uint32_t)wf.size; /* protocol 2 (docs/shm-protocol.md) */
    memcpy(hdr, "PWGH", 4);
    memcpy(hdr + 4, &one, 4);
    memcpy(hdr + 8, &len, 4);
    write(fd, hdr, 12);
    write(fd, wf.data, wf.size);
    if (read_full(fd, hdr, 12) || memcmp(hdr, "PWGR", 4)) { fprintf(stderr, "no PWGR\n"); return 1; }
    printf("connected in %.3f s: %d workers on the far side\n", now() - t0, rd32(hdr + 8));

    /* one urgent round trip first, for the latency of a lone request */
    uint8_t q[212];
    memset(q, 0, sizeof q);
    memcpy(q, &cs[0].cx, 4);
    memcpy(q + 4, &cs[0].cz, 4);
    uint32_t urgent = 1;
    memcpy(q + 8, &urgent, 4);
    for (int i = 0; i < 100; i++) memcpy(q + 12 + 2 * i, &cs[0].bi10[i], 2);
    static uint8_t buf[135168];
    uint8_t rh[16];
    double tl = now();
    write(fd, q, sizeof q);
    if (read_full(fd, rh, 16) || read_full(fd, buf, (size_t)rd32(rh + 8))) { fprintf(stderr, "lost\n"); return 1; }
    printf("one urgent request: %.3f ms round trip, %.3f ms of it compute on the far side, %d bytes\n",
           1e3 * (now() - tl), rd32(rh + 12) / 1e3, rd32(rh + 8));

    long bad = 0, total = 0, bytes = 0;
    double tb = now();
    for (int r = 0; r < rounds; r++) {
        for (int i = 0; i < n; i++) {
            memcpy(q, &cs[i].cx, 4);
            memcpy(q + 4, &cs[i].cz, 4);
            uint32_t zero = 0;
            memcpy(q + 8, &zero, 4);
            for (int k = 0; k < 100; k++) memcpy(q + 12 + 2 * k, &cs[i].bi10[k], 2);
            write(fd, q, sizeof q);
        }
        for (int got = 0; got < n; got++) {
            if (read_full(fd, rh, 16)) { fprintf(stderr, "lost\n"); return 1; }
            uint32_t l = (uint32_t)rd32(rh + 8);
            if (read_full(fd, buf, l)) { fprintf(stderr, "lost\n"); return 1; }
            bytes += l;
            int cx = rd32(rh), cz = rd32(rh + 4), i;
            for (i = 0; i < n && !(cs[i].cx == cx && cs[i].cz == cz); i++) {}
            if (i == n) { bad++; continue; }
            /* decode the column runs and compare with SHAP */
            int c = 0, y = 0, ok = 1;
            /* the payload: u32 flags, the 16 x 16 biome slots (flag 1), then the runs; the biomes go with the requests here */
            uint32_t p0 = 4 + ((rd32(buf) & 1) ? 512 : 0);
            for (uint32_t p = p0; p + 1 < l && c < 256; p += 2) {
                uint16_t id = buf[p] == 1 ? BLOCK_STONE : buf[p] == 2 ? BLOCK_WATER : BLOCK_NULL;
                for (int k = 0; k <= buf[p + 1]; k++, y++)
                    if (cs[i].want[(c << 8) | y] != id) ok = 0;
                if (y == 256) { y = 0; c++; }
            }
            if (c != 256) ok = 0;
            if (!ok && bad++ == 0) printf("first difference: chunk %d,%d\n", cx, cz);
            total++;
        }
    }
    double s = now() - tb;
    printf("%ld chunks in %.3f s: %.1f chunks/s, %.0f bytes a chunk, %ld differ from the capture\n", total, s,
           total / s, (double)bytes / (total ? total : 1), bad);
    close(fd);
    return bad ? 1 : 0;
}
