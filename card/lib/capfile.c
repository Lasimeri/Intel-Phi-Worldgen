/* capfile.c: reading the capture files of docs/capture-format.md. See capfile.md. */
#include "capfile.h"

#include <stdio.h>
#include <stdlib.h>

int cap_load(capfile *f, const char *path, const char magic[4])
{
    FILE *in = fopen(path, "rb");
    if (!in) { perror(path); return -1; }
    fseek(in, 0, SEEK_END);
    long n = ftell(in);
    fseek(in, 0, SEEK_SET);
    f->data = malloc(n > 0 ? (size_t)n : 1);
    f->size = (size_t)n;
    if (!f->data || fread(f->data, 1, f->size, in) != f->size) {
        fprintf(stderr, "%s: short read\n", path);
        fclose(in);
        return -1;
    }
    fclose(in);
    if (f->size < 8 || memcmp(f->data, magic, 4) != 0 || rd32(f->data + 4) != 1) {
        fprintf(stderr, "%s: not a version 1 %.4s file\n", path, magic);
        return -1;
    }
    return 0;
}

int cap_wrap(capfile *f, uint8_t *data, size_t size, const char magic[4])
{
    f->data = data;
    f->size = size;
    if (size < 8 || memcmp(data, magic, 4) != 0 || rd32(data + 4) != 1) {
        fprintf(stderr, "buffer: not a version 1 %.4s blob\n", magic);
        return -1;
    }
    return 0;
}

void cap_free(capfile *f)
{
    free(f->data);
    f->data = NULL;
    f->size = 0;
}

const uint8_t *cap_section(const capfile *f, const char tag[4], uint32_t *len)
{
    size_t at = 8;
    while (at + 8 <= f->size) {
        uint32_t n = (uint32_t)rd32(f->data + at + 4);
        if (memcmp(f->data + at, tag, 4) == 0 && at + 8 + n <= f->size) {
            if (len) *len = n;
            return f->data + at + 8;
        }
        at += 8 + (size_t)n;
    }
    return NULL;
}

const uint8_t *cap_need(const capfile *f, const char *path, const char tag[4], uint32_t want)
{
    uint32_t n = 0;
    const uint8_t *p = cap_section(f, tag, &n);
    if (!p || n < want) {
        fprintf(stderr, "%s: section %.4s missing or %u bytes, want %u\n", path, tag, p ? n : 0, want);
        exit(2);
    }
    return p;
}
