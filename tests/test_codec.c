/*
 * Reference codec: roundtrip, both tile paths, determinism, and the
 * re-encode fixed point the store-identity check depends on.
 */
#include "fixture.h"
#include "harness.h"
#include "mzg2/adapters/kimi_k3.h"
#include "mzg2/mzg2.h"

static void test_fixture_roundtrip(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t decoded[MZG2_FIXTURE_OUTPUT_BYTES];
    mzg2_encoded encoded;
    mzg2_block_view view;
    char error[256];
    uint32_t flags;
    uint32_t tile;
    uint32_t coded_payload = 0u;
    uint32_t raw_payload = 0u;

    printf("fixture roundtrip\n");
    mzg2_fixture_fill(payload, MZG2_FIXTURE_SEED);
    CHECK_MSG(mzg2_fixture_encode(payload, 1u, 0u, &encoded, error,
                                  sizeof(error)),
              "encode: %s", error);
    if (!encoded.bytes) {
        return;
    }
    CHECK(encoded.tile_count == 8u);
    CHECK(encoded.size % MZG2_ALIGNMENT == 0u);
    /* Both paths must appear, or the fixture stopped testing one of them. */
    CHECK_MSG(encoded.raw_tiles > 0u && encoded.raw_tiles < encoded.tile_count,
              "raw tiles %u of %u", encoded.raw_tiles, encoded.tile_count);

    CHECK_MSG(mzg2_block_validate(profile, 1u, 0u, encoded.bytes,
                                  encoded.size, &view, error, sizeof(error)),
              "validate: %s", error);
    CHECK(view.header->model_offset == sizeof(mzg2_block_header));
    CHECK(view.header->descriptor_offset ==
          sizeof(mzg2_block_header) + sizeof(mzg2_model_entry));
    CHECK(view.header->descriptor_offset % 8u == 0u);
    CHECK(view.header->payload_offset % MZG2_ALIGNMENT == 0u);
    for (tile = 0u; tile < view.tile_count; ++tile) {
        const mzg2_tile_descriptor *descriptor = &view.descriptors[tile];

        CHECK(descriptor->payload_offset % 4u == 0u);
        CHECK(descriptor->output_offset == tile * MZG2_FIXTURE_TILE_BYTES);
        CHECK(descriptor->output_bytes == MZG2_FIXTURE_TILE_BYTES);
        CHECK((descriptor->checksum >> 32u) == 0u);
        if (descriptor->kind & MZG2_TILE_RAW) {
            raw_payload += descriptor->payload_bytes;
        } else {
            coded_payload += descriptor->payload_bytes;
        }
    }
    /* The compressible half must actually compress. */
    CHECK_MSG(coded_payload <
                  (view.tile_count - encoded.raw_tiles) *
                      MZG2_FIXTURE_TILE_BYTES,
              "coded %u bytes for %u tiles", coded_payload,
              view.tile_count - encoded.raw_tiles);
    CHECK(raw_payload == encoded.raw_tiles * MZG2_FIXTURE_TILE_BYTES);

    memset(decoded, 0xa5, sizeof(decoded));
    flags = mzg2_decode_block(profile, 1u, 0u, encoded.bytes, encoded.size,
                              decoded, sizeof(decoded), error, sizeof(error));
    CHECK_MSG(flags == 0u, "decode flags %u: %s", flags, error);
    CHECK(memcmp(payload, decoded, sizeof(payload)) == 0);
    mzg2_encoded_release(&encoded);
}

static void test_determinism_and_fixed_point(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t decoded[MZG2_FIXTURE_OUTPUT_BYTES];
    mzg2_encoded first;
    mzg2_encoded second;
    mzg2_encoded again;
    char error[256];

    printf("determinism and re-encode fixed point\n");
    mzg2_fixture_fill(payload, MZG2_FIXTURE_SEED + 1u);
    CHECK(mzg2_fixture_encode(payload, 2u, 3u, &first, error, sizeof(error)));
    CHECK(mzg2_fixture_encode(payload, 2u, 3u, &second, error, sizeof(error)));
    if (!first.bytes || !second.bytes) {
        return;
    }
    CHECK(first.size == second.size);
    CHECK(memcmp(first.bytes, second.bytes, first.size) == 0);

    /*
     * Decode, then encode the decoded payload again. Byte identity here is
     * what lets a re-encode of a stored block prove the extracted codec
     * reproduces that store: encode(decode(block)) == block.
     */
    CHECK(mzg2_decode_block(profile, 2u, 3u, first.bytes, first.size, decoded,
                            sizeof(decoded), error, sizeof(error)) == 0u);
    CHECK(mzg2_fixture_encode(decoded, 2u, 3u, &again, error, sizeof(error)));
    if (again.bytes) {
        CHECK(again.size == first.size);
        CHECK(memcmp(again.bytes, first.bytes, first.size) == 0);
    }
    mzg2_encoded_release(&first);
    mzg2_encoded_release(&second);
    mzg2_encoded_release(&again);
}

static void test_destination_bounds(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t decoded[MZG2_FIXTURE_OUTPUT_BYTES];
    mzg2_encoded encoded;
    char error[256];

    printf("destination bounds\n");
    mzg2_fixture_fill(payload, MZG2_FIXTURE_SEED + 2u);
    CHECK(mzg2_fixture_encode(payload, 1u, 1u, &encoded, error,
                              sizeof(error)));
    if (!encoded.bytes) {
        return;
    }
    CHECK(mzg2_decode_block(profile, 1u, 1u, encoded.bytes, encoded.size,
                            decoded, sizeof(decoded) - 1u, error,
                            sizeof(error)) == MZG2_ERROR_BOUNDS);
    /* The identity must match the block. */
    CHECK(mzg2_decode_block(profile, 1u, 2u, encoded.bytes, encoded.size,
                            decoded, sizeof(decoded), error,
                            sizeof(error)) == MZG2_ERROR_BOUNDS);
    CHECK(mzg2_decode_block(profile, 2u, 1u, encoded.bytes, encoded.size,
                            decoded, sizeof(decoded), error,
                            sizeof(error)) == MZG2_ERROR_BOUNDS);
    /* Out-of-profile identities are refused before anything is read. */
    CHECK(mzg2_decode_block(profile, 0u, 1u, encoded.bytes, encoded.size,
                            decoded, sizeof(decoded), error,
                            sizeof(error)) == MZG2_ERROR_BOUNDS);
    CHECK(mzg2_decode_block(profile, 1u, MZG2_FIXTURE_EXPERTS, encoded.bytes,
                            encoded.size, decoded, sizeof(decoded), error,
                            sizeof(error)) == MZG2_ERROR_BOUNDS);
    mzg2_encoded_release(&encoded);
}

/*
 * The real profile's geometry -- 1071 tiles of 16 KiB, six planes, two
 * models -- on synthetic data. No model weights, no licence: this checks the
 * geometry and the adapter's plane layout, not Kimi K3's values.
 */
static void test_k3_geometry_roundtrip(void)
{
    const mzg2_profile *profile = mzg2_kimi_k3_profile_unbound();
    static const uint8_t palette[15] = { 0x00u, 0x10u, 0x20u, 0x30u, 0x40u,
                                         0x50u, 0x60u, 0x70u, 0x80u, 0x90u,
                                         0xa0u, 0xb0u, 0xc0u, 0xd0u, 0xffu };
    uint8_t *payload;
    uint8_t *decoded;
    const mzg2_plane *planes;
    mzg2_built_model models[MZG2_MAX_MODELS];
    mzg2_model_entry entry;
    mzg2_encoded encoded;
    mzg2_prng prng;
    uint32_t plane_count = 0u;
    uint32_t plane;
    char error[256];
    uint32_t flags;

    printf("K3 geometry roundtrip (synthetic payload)\n");
    payload = (uint8_t *)malloc(MZG2_K3_EXPERT_BYTES);
    decoded = (uint8_t *)malloc(MZG2_K3_EXPERT_BYTES);
    if (!payload || !decoded) {
        printf("  skipped: allocation failed\n");
        free(payload);
        free(decoded);
        return;
    }
    planes = mzg2_kimi_k3_planes(&plane_count);
    mzg2_prng_seed(&prng, MZG2_FIXTURE_SEED);
    for (plane = 0u; plane < plane_count; ++plane) {
        uint8_t *cursor = payload + planes[plane].offset;
        uint32_t offset;

        if (planes[plane].model == MZG2_K3_MODEL_PACKED) {
            for (offset = 0u; offset < planes[plane].bytes; ++offset) {
                const uint32_t draw = mzg2_prng_below(&prng, 100u);

                cursor[offset] = draw < 60u
                                     ? 0x00u
                                     : (uint8_t)mzg2_prng_next(&prng);
            }
        } else {
            for (offset = 0u; offset < planes[plane].bytes; ++offset) {
                cursor[offset] = palette[mzg2_prng_below(&prng, 15u)];
            }
        }
    }
    /* Packed planes must carry no negative-zero code before encoding. */
    CHECK(mzg2_kimi_k3_canonicalize_expert(payload, MZG2_K3_EXPERT_BYTES) > 0u);
    CHECK(mzg2_kimi_k3_canonicalize_expert(payload, MZG2_K3_EXPERT_BYTES) ==
          0u);

    CHECK_MSG(mzg2_models_from_planes(profile, payload, planes, plane_count,
                                      models, &entry, error, sizeof(error)),
              "models: %s", error);
    CHECK(entry.model[MZG2_K3_MODEL_PACKED].symbol_count == 15u);
    CHECK(entry.model[MZG2_K3_MODEL_SCALE].symbol_count == 15u);
    CHECK_MSG(mzg2_encode_block(profile, 92u, 895u, payload, planes,
                                plane_count, models, &entry, &encoded, error,
                                sizeof(error)),
              "encode: %s", error);
    if (encoded.bytes) {
        CHECK(encoded.tile_count == 1071u);
        CHECK(encoded.size % MZG2_ALIGNMENT == 0u);
        memset(decoded, 0xa5, MZG2_K3_EXPERT_BYTES);
        flags = mzg2_decode_block(profile, 92u, 895u, encoded.bytes,
                                  encoded.size, decoded, MZG2_K3_EXPERT_BYTES,
                                  error, sizeof(error));
        CHECK_MSG(flags == 0u, "decode flags %u: %s", flags, error);
        CHECK(memcmp(payload, decoded, MZG2_K3_EXPERT_BYTES) == 0);
    }
    mzg2_encoded_release(&encoded);
    free(payload);
    free(decoded);
}

int main(void)
{
    test_fixture_roundtrip();
    test_determinism_and_fixed_point();
    test_destination_bounds();
    test_k3_geometry_roundtrip();
    return mzg2_test_report("test_codec");
}
