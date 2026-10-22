/* capfile.h: reading the capture files of docs/capture-format.md. See capfile.md. */
#ifndef CAPFILE_H
#define CAPFILE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t *data;
    size_t size;
} capfile;

/* Reads path whole; 0 when it has the 4-byte magic and version 1, else -1 with a message on stderr. */
int cap_load(capfile *f, const char *path, const char magic[4]);

/* The same checks on a buffer already in memory (taken over: cap_free frees it). */
int cap_wrap(capfile *f, uint8_t *data, size_t size, const char magic[4]);
void cap_free(capfile *f);

/* The payload of the section with this tag and its length, or NULL. */
const uint8_t *cap_section(const capfile *f, const char tag[4], uint32_t *len);

/* The same, but exits with a message when the section is missing or shorter than want bytes. */
const uint8_t *cap_need(const capfile *f, const char *path, const char tag[4], uint32_t want);

/* Little-endian reads (the card and the host are both little-endian x86-64). */
static inline uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline int32_t rd32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static inline int64_t rd64(const uint8_t *p) { int64_t v; memcpy(&v, p, 8); return v; }
static inline float rdf32(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static inline double rdf64(const uint8_t *p) { double v; memcpy(&v, p, 8); return v; }

#endif
