/*
 * Minimal test harness: no framework, no network, no model weights.
 *
 * Fixture payloads are generated from a fixed seed, so every test is
 * reproducible on any machine. The "fixture checksum" below is FNV-1a, not a
 * cryptographic digest: it anchors fixtures against drift. Byte-level claims
 * in these tests are made with memcmp over complete buffers, never a hash.
 */
#ifndef MZG2_TESTS_HARNESS_H
#define MZG2_TESTS_HARNESS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mzg2_test_failures;
static int mzg2_test_checks;

#define CHECK(condition)                                                      \
    do {                                                                      \
        ++mzg2_test_checks;                                                   \
        if (!(condition)) {                                                   \
            ++mzg2_test_failures;                                             \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition);     \
        }                                                                     \
    } while (0)

#define CHECK_MSG(condition, ...)                                             \
    do {                                                                      \
        ++mzg2_test_checks;                                                   \
        if (!(condition)) {                                                   \
            ++mzg2_test_failures;                                             \
            printf("  FAIL %s:%d  ", __FILE__, __LINE__);                     \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
        }                                                                     \
    } while (0)

static inline int mzg2_test_report(const char *name)
{
    printf("%s: %d checks, %d failures\n", name, mzg2_test_checks,
           mzg2_test_failures);
    return mzg2_test_failures == 0 ? 0 : 1;
}

/* xorshift64*: small, deterministic, and good enough for fixtures. */
typedef struct {
    uint64_t state;
} mzg2_prng;

static inline void mzg2_prng_seed(mzg2_prng *prng, uint64_t seed)
{
    prng->state = seed ? seed : 0x9e3779b97f4a7c15ull;
}

static inline uint64_t mzg2_prng_next(mzg2_prng *prng)
{
    uint64_t x = prng->state;

    x ^= x >> 12u;
    x ^= x << 25u;
    x ^= x >> 27u;
    prng->state = x;
    return x * 0x2545f4914f6cdd1dull;
}

static inline uint32_t mzg2_prng_below(mzg2_prng *prng, uint32_t bound)
{
    return (uint32_t)(mzg2_prng_next(prng) % bound);
}

static inline uint64_t mzg2_fixture_checksum(const void *data, size_t bytes)
{
    const uint8_t *cursor = (const uint8_t *)data;
    uint64_t hash = 0xcbf29ce484222325ull;
    size_t index;

    for (index = 0u; index < bytes; ++index) {
        hash ^= cursor[index];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static inline uint8_t *mzg2_test_read_file(const char *path, size_t *bytes)
{
    FILE *file = fopen(path, "rb");
    uint8_t *data;
    long size;

    *bytes = 0u;
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    data = (uint8_t *)malloc((size_t)size);
    if (!data || fread(data, 1u, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *bytes = (size_t)size;
    return data;
}

#endif /* MZG2_TESTS_HARNESS_H */
