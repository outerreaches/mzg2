/*
 * The golden vector catalogue.
 *
 * Each vector is a complete MZG2 block built from a payload this header
 * generates, so an independent implementation can decode the committed bytes
 * and compare its output against a payload it reproduces from the same
 * description -- no model weights, no licence, no host state.
 *
 * Three payloads, chosen for the paths they force:
 *   mixed       both coded tiles and raw escapes in one block
 *   uniform     each plane uniform over its own alphabet. The 4-bit plane
 *               escapes to raw -- 15 equiprobable symbols need 3.91 of its
 *               4 bits, and the 128-byte state header then exceeds the
 *               saving -- while the 8-bit plane still codes to about half
 *               size. A restricted alphabet in a wide symbol is compressible
 *               however it is distributed; in a narrow one it is not.
 *   degenerate  one symbol per plane: the entropy coder's identity case,
 *               where no renormalization word is ever emitted and a coded
 *               tile is 128 bytes of initial states
 */
#ifndef MZG2_TESTS_VECTORS_H
#define MZG2_TESTS_VECTORS_H

#include "fixture.h"
#include "harness.h"

#define MZG2_VECTOR_COUNT 3
#define MZG2_VECTOR_MANIFEST "MANIFEST"

typedef void (*mzg2_vector_fill)(uint8_t *payload);

typedef struct {
    const char *name;
    mzg2_vector_fill fill;
    uint32_t layer;
    uint32_t expert;
} mzg2_vector;

static inline void mzg2_vector_fill_mixed(uint8_t *payload)
{
    mzg2_fixture_fill(payload, MZG2_FIXTURE_SEED);
}

/* Uniform over each plane's alphabet. */
static inline void mzg2_vector_fill_uniform(uint8_t *payload)
{
    mzg2_prng prng;
    uint32_t offset;

    mzg2_prng_seed(&prng, MZG2_FIXTURE_SEED + 101u);
    for (offset = 0u; offset < MZG2_FIXTURE_PLANE_BYTES; ++offset) {
        const uint8_t low = mzg2_fixture_uniform_nibble(&prng);
        const uint8_t high = mzg2_fixture_uniform_nibble(&prng);

        payload[offset] = (uint8_t)(low | (high << 4u));
    }
    for (offset = 0u; offset < MZG2_FIXTURE_PLANE_BYTES; ++offset) {
        payload[MZG2_FIXTURE_PLANE_BYTES + offset] =
            mzg2_fixture_palette[mzg2_prng_below(&prng, 15u)];
    }
}

/* One symbol per plane. The rANS step becomes the identity, so states stay
 * at the lower bound and the word stream is empty. */
static inline void mzg2_vector_fill_degenerate(uint8_t *payload)
{
    memset(payload, 0x00u, MZG2_FIXTURE_PLANE_BYTES);
    memset(payload + MZG2_FIXTURE_PLANE_BYTES, mzg2_fixture_palette[5],
           MZG2_FIXTURE_PLANE_BYTES);
}

static const mzg2_vector mzg2_vector_catalogue[MZG2_VECTOR_COUNT] = {
    { "fixture-mixed", mzg2_vector_fill_mixed, 1u, 0u },
    { "fixture-uniform", mzg2_vector_fill_uniform, 1u, 1u },
    { "fixture-degenerate", mzg2_vector_fill_degenerate, 2u, 3u }
};

static inline void mzg2_vector_path(char *buffer, size_t size,
                                    const char *directory, const char *name)
{
    snprintf(buffer, size, "%s/%s.mzg2blk", directory, name);
}

#endif /* MZG2_TESTS_VECTORS_H */
