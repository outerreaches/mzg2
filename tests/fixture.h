/*
 * A synthetic profile and payload generator.
 *
 * Deliberately not Kimi K3: small geometry, its own magic, no source
 * binding. It exercises the core's generality (a restricted 4-bit alphabet
 * beside an 8-bit one) with no model licence attached, and it is what the
 * golden vectors are built from.
 *
 * The payload mixes compressible and incompressible regions on purpose, so
 * both the coded path and the raw escape appear in every run.
 */
#ifndef MZG2_TESTS_FIXTURE_H
#define MZG2_TESTS_FIXTURE_H

#include "harness.h"
#include "mzg2/mzg2.h"

enum {
    MZG2_FIXTURE_TILE_BYTES = 1024,
    MZG2_FIXTURE_OUTPUT_BYTES = 8192,
    MZG2_FIXTURE_PLANE_BYTES = 4096,
    MZG2_FIXTURE_LAYERS = 2,
    MZG2_FIXTURE_EXPERTS = 4,
    MZG2_FIXTURE_SEED = 20261020
};

/* 4-bit alphabet with one code removed, mirroring a value transform that
 * folds a redundant code away. */
static const uint8_t mzg2_fixture_nibble_allowed[32] = {
    0xffu, 0xfeu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

static const mzg2_profile mzg2_fixture_profile_value = {
    .magic = "MZG2TST",
    .sidecar_pattern = "layer-%03u.tst",
    .tile_bytes = MZG2_FIXTURE_TILE_BYTES,
    .output_bytes = MZG2_FIXTURE_OUTPUT_BYTES,
    .max_layers = MZG2_FIXTURE_LAYERS,
    .experts_per_layer = MZG2_FIXTURE_EXPERTS,
    .model_count = 2,
    .model = { { .symbol_bits = 4, .allowed = mzg2_fixture_nibble_allowed },
               { .symbol_bits = 8, .allowed = NULL } },
    .source_manifest_sha256 = NULL,
    .require_complete_directory = false
};

static const mzg2_plane mzg2_fixture_plane_value[2] = {
    { 0u, MZG2_FIXTURE_PLANE_BYTES, 0u },
    { MZG2_FIXTURE_PLANE_BYTES, MZG2_FIXTURE_PLANE_BYTES, 1u }
};

/* Fifteen byte values for the 8-bit plane: the whole plane shares one
 * palette, so the built alphabet stays inside the format's 15 symbols. */
static const uint8_t mzg2_fixture_palette[15] = {
    0x00u, 0x01u, 0x02u, 0x03u, 0x07u, 0x0fu, 0x1fu, 0x3fu,
    0x7fu, 0x80u, 0xc0u, 0xe0u, 0xf0u, 0xfeu, 0xffu
};

static inline const mzg2_profile *mzg2_fixture_profile(void)
{
    return &mzg2_fixture_profile_value;
}

static inline const mzg2_plane *mzg2_fixture_planes(uint32_t *count)
{
    if (count) {
        *count = 2u;
    }
    return mzg2_fixture_plane_value;
}

/* A nibble from the allowed alphabet, skewed towards low codes. */
static inline uint8_t mzg2_fixture_skewed_nibble(mzg2_prng *prng)
{
    const uint32_t draw = mzg2_prng_below(prng, 100u);
    uint8_t value;

    if (draw < 70u) {
        value = 0u;
    } else if (draw < 85u) {
        value = 1u;
    } else if (draw < 93u) {
        value = 2u;
    } else {
        value = (uint8_t)(3u + mzg2_prng_below(prng, 13u)); /* 3..15 */
    }
    if (value == 8u) {
        value = 15u; /* stay inside the alphabet */
    }
    return value;
}

static inline uint8_t mzg2_fixture_uniform_nibble(mzg2_prng *prng)
{
    const uint8_t choice = (uint8_t)mzg2_prng_below(prng, 15u);

    return choice < 8u ? choice : (uint8_t)(choice + 1u); /* skips 8 */
}

/*
 * Fill one fixture payload. Per plane: the first half is compressible, the
 * second half is uniform over the alphabet and therefore escapes to raw.
 */
static inline void mzg2_fixture_fill(uint8_t *payload, uint64_t seed)
{
    mzg2_prng prng;
    uint32_t offset;

    mzg2_prng_seed(&prng, seed);
    for (offset = 0u; offset < MZG2_FIXTURE_PLANE_BYTES; ++offset) {
        const bool compressible = offset < MZG2_FIXTURE_PLANE_BYTES / 2u;
        uint8_t low;
        uint8_t high;

        if (compressible) {
            low = mzg2_fixture_skewed_nibble(&prng);
            high = mzg2_fixture_skewed_nibble(&prng);
        } else {
            low = mzg2_fixture_uniform_nibble(&prng);
            high = mzg2_fixture_uniform_nibble(&prng);
        }
        payload[offset] = (uint8_t)(low | (high << 4u));
    }
    for (offset = 0u; offset < MZG2_FIXTURE_PLANE_BYTES; ++offset) {
        const bool compressible = offset < MZG2_FIXTURE_PLANE_BYTES / 2u;
        uint32_t index;

        if (compressible) {
            const uint32_t draw = mzg2_prng_below(&prng, 100u);

            index = draw < 75u ? 0u : (draw < 90u ? 1u : 2u + draw % 13u);
            if (index > 14u) {
                index = 14u;
            }
        } else {
            index = mzg2_prng_below(&prng, 15u);
        }
        payload[MZG2_FIXTURE_PLANE_BYTES + offset] =
            mzg2_fixture_palette[index];
    }
}

/* Encode one fixture block. Returns false with a message on failure. */
static inline bool mzg2_fixture_encode(const uint8_t *payload, uint32_t layer,
                                       uint32_t expert, mzg2_encoded *out,
                                       char *error, size_t error_size)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    mzg2_built_model models[MZG2_MAX_MODELS];
    mzg2_model_entry entry;
    uint32_t plane_count;
    const mzg2_plane *planes = mzg2_fixture_planes(&plane_count);

    if (!mzg2_models_from_planes(profile, payload, planes, plane_count, models,
                                 &entry, error, error_size)) {
        return false;
    }
    return mzg2_encode_block(profile, layer, expert, payload, planes,
                             plane_count, models, &entry, out, error,
                             error_size);
}

#endif /* MZG2_TESTS_FIXTURE_H */
