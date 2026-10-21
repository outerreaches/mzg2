/* Kimi K3 adapter: profile constants, plane layout, zero-gauge transform. */
#include "mzg2/adapters/kimi_k3.h"

#include <string.h>

/*
 * Packed planes carry MXFP4 nibbles, so symbols are 4 bits wide and the
 * alphabet is 0..15 minus the negative-zero code the transform removes.
 * Expressed as a value bitmap, which is all the core understands.
 */
static const uint8_t k3_packed_allowed[32] = {
    /* values 0..7 permitted; value 8 (E2M1 -0) removed; 9..15 permitted */
    0xffu, 0xfeu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

/*
 * SHA-256 of the pinned Kimi K3 source manifest the qualified store was
 * derived from. A digest of a public manifest, not a secret; it exists so a
 * store states which source revision produced it.
 */
static const uint8_t k3_source_manifest[32] = {
    0x47u, 0x6fu, 0xa0u, 0xbau, 0x64u, 0xe3u, 0x23u, 0x3cu,
    0xbbu, 0x9cu, 0xa0u, 0x64u, 0x23u, 0x27u, 0x36u, 0x1au,
    0x73u, 0xf6u, 0x80u, 0x7eu, 0x75u, 0x1eu, 0xdbu, 0x07u,
    0x1cu, 0x92u, 0xfau, 0x22u, 0x16u, 0xb2u, 0x02u, 0xa4u
};

#define K3_PROFILE_FIELDS                                                     \
    .magic = "K3MZG2",                                                        \
    .sidecar_pattern = "layer-%03u.mzg2",                                     \
    .tile_bytes = MZG2_K3_TILE_BYTES,                                         \
    .output_bytes = MZG2_K3_EXPERT_BYTES,                                     \
    .max_layers = MZG2_K3_LAYERS,                                             \
    .experts_per_layer = MZG2_K3_EXPERTS_PER_LAYER,                           \
    .model_count = 2,                                                         \
    .model = {                                                                \
        [MZG2_K3_MODEL_PACKED] = { .symbol_bits = 4,                          \
                                   .allowed = k3_packed_allowed },            \
        [MZG2_K3_MODEL_SCALE] = { .symbol_bits = 8, .allowed = NULL }         \
    },                                                                        \
    .require_complete_directory = true

static const mzg2_profile k3_profile = {
    K3_PROFILE_FIELDS,
    .source_manifest_sha256 = k3_source_manifest
};

static const mzg2_profile k3_profile_unbound = {
    K3_PROFILE_FIELDS,
    .source_manifest_sha256 = NULL
};

/*
 * Plane order is the physical order of the six routed tensors inside one
 * expert's contiguous span in the source checkpoint.
 */
static const mzg2_plane k3_planes[MZG2_K3_PLANES] = {
    { 0u, MZG2_K3_PACKED_BYTES, MZG2_K3_MODEL_PACKED },
    { 5505024u, MZG2_K3_SCALE_BYTES, MZG2_K3_MODEL_SCALE },
    { 5849088u, MZG2_K3_PACKED_BYTES, MZG2_K3_MODEL_PACKED },
    { 11354112u, MZG2_K3_SCALE_BYTES, MZG2_K3_MODEL_SCALE },
    { 11698176u, MZG2_K3_PACKED_BYTES, MZG2_K3_MODEL_PACKED },
    { 17203200u, MZG2_K3_SCALE_BYTES, MZG2_K3_MODEL_SCALE }
};

static const char *const k3_plane_suffixes[MZG2_K3_PLANES] = {
    "w1.weight_packed", "w1.weight_scale", "w2.weight_packed",
    "w2.weight_scale", "w3.weight_packed", "w3.weight_scale"
};

const mzg2_profile *mzg2_kimi_k3_profile(void)
{
    return &k3_profile;
}

const mzg2_profile *mzg2_kimi_k3_profile_unbound(void)
{
    return &k3_profile_unbound;
}

const uint8_t *mzg2_kimi_k3_source_manifest_sha256(void)
{
    return k3_source_manifest;
}

const mzg2_plane *mzg2_kimi_k3_planes(uint32_t *count)
{
    if (count) {
        *count = MZG2_K3_PLANES;
    }
    return k3_planes;
}

const char *const *mzg2_kimi_k3_plane_suffixes(uint32_t *count)
{
    if (count) {
        *count = MZG2_K3_PLANES;
    }
    return k3_plane_suffixes;
}

uint64_t mzg2_kimi_k3_canonicalize_packed(void *data, uint32_t bytes)
{
    uint8_t *cursor = (uint8_t *)data;
    uint64_t changed = 0u;
    uint32_t index;

    if (!cursor) {
        return 0u;
    }
    for (index = 0u; index < bytes; ++index) {
        uint8_t low = cursor[index] & 0x0fu;
        uint8_t high = (uint8_t)(cursor[index] >> 4u);

        if (low == MZG2_K3_NEGATIVE_ZERO_NIBBLE) {
            low = 0u;
            ++changed;
        }
        if (high == MZG2_K3_NEGATIVE_ZERO_NIBBLE) {
            high = 0u;
            ++changed;
        }
        cursor[index] = (uint8_t)(low | (high << 4u));
    }
    return changed;
}

uint64_t mzg2_kimi_k3_canonicalize_expert(void *expert, uint32_t bytes)
{
    uint8_t *cursor = (uint8_t *)expert;
    uint64_t changed = 0u;
    uint32_t plane;

    if (!cursor || bytes < MZG2_K3_EXPERT_BYTES) {
        return 0u;
    }
    for (plane = 0u; plane < MZG2_K3_PLANES; ++plane) {
        if (k3_planes[plane].model != MZG2_K3_MODEL_PACKED) {
            continue;
        }
        changed += mzg2_kimi_k3_canonicalize_packed(
            cursor + k3_planes[plane].offset, k3_planes[plane].bytes);
    }
    return changed;
}
