/* Vary real payload content and tile geometry, including short final tiles. */
#include "fixture.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    mzg2_profile profile = *mzg2_fixture_profile();
    mzg2_plane plane;
    mzg2_built_model models[MZG2_MAX_MODELS];
    mzg2_model_entry entry;
    mzg2_encoded encoded = {0};
    uint8_t source[8192], decoded[8192];
    char error[256];
    uint32_t i;

    if (size < 3u || size > sizeof(source)) return 0;
    profile.model_count = 1u;
    profile.model[0].symbol_bits = (data[0] & 1u) ? 4u : 8u;
    profile.model[0].allowed = NULL;
    profile.tile_bytes = 128u << (data[1] % 5u);
    profile.output_bytes = 128u * (1u + data[2] % 64u);
    plane = (mzg2_plane){0u, profile.output_bytes, 0u};
    for (i = 0; i < profile.output_bytes; ++i) {
        const uint8_t byte = data[i % size];
        source[i] = profile.model[0].symbol_bits == 4u
            ? (uint8_t)((byte % 15u) | (((byte / 15u) % 15u) << 4u))
            : mzg2_fixture_palette[byte % 15u];
    }
    if (!mzg2_models_from_planes(&profile, source, &plane, 1u, models,
                                &entry, error, sizeof(error)) ||
        !mzg2_encode_block(&profile, 1u, 0u, source, &plane, 1u, models,
                          &entry, &encoded, error, sizeof(error))) abort();
    if (mzg2_decode_block(&profile, 1u, 0u, encoded.bytes, encoded.size,
        decoded, sizeof(decoded), error, sizeof(error)) != 0u ||
        memcmp(source, decoded, profile.output_bytes) != 0) abort();
    mzg2_encoded_release(&encoded);
    return 0;
}
