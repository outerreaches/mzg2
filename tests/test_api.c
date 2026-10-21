/* Public API boundaries found while preparing sanitizer/fuzz coverage. */
#include "fixture.h"

static void test_arithmetic(void)
{
    mzg2_profile profile = *mzg2_fixture_profile();
    mzg2_built_model model;
    mzg2_model_spec spec = {8u, NULL};
    uint64_t counts[256] = {0};
    char error[256];

    profile.output_bytes = UINT32_MAX - 127u;
    CHECK(mzg2_profile_tile_count(&profile) == 4194304u);
    CHECK(mzg2_profile_check(&profile, error, sizeof(error)));
    CHECK(mzg2_profile_tile_output_bytes(&profile, 4194303u) == 896u);
    counts[0] = UINT64_MAX;
    counts[1] = 2u;
    CHECK(!mzg2_model_build(counts, &spec, &model));
    counts[0] = UINT64_MAX / MZG2_MODEL_SLOTS;
    counts[1] = 0u;
    CHECK(mzg2_model_build(counts, &spec, &model));
    CHECK(mzg2_model_validate(&model.table, &spec));
    counts[1] = 1u;
    CHECK(!mzg2_model_build(counts, &spec, &model));
    spec.symbol_bits = 255u;
    CHECK(!mzg2_model_build(counts, &spec, &model));
    CHECK(!mzg2_model_validate(&model.table, &spec));
}

static void test_buffers_and_profiles(void)
{
    mzg2_profile profile = *mzg2_fixture_profile();
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t output[MZG2_FIXTURE_OUTPUT_BYTES];
    mzg2_encoded block = {0}, rejected = {0};
    mzg2_block_view view;
    mzg2_built_model models[MZG2_MAX_MODELS];
    mzg2_model_entry entry;
    mzg2_tile_rules rules;
    mzg2_plane planes[2];
    char error[256];
    uint8_t *storage, *saved;
    size_t bytes;

    mzg2_fixture_fill(payload, MZG2_FIXTURE_SEED);
    CHECK(mzg2_fixture_encode(payload, 1u, 0u, &block, error, sizeof(error)));
    if (!block.bytes) return;
    bytes = block.size + sizeof(output) + 8u;
    storage = (uint8_t *)malloc(bytes);
    saved = (uint8_t *)malloc(bytes);
    CHECK(storage != NULL && saved != NULL);
    if (!storage || !saved) goto done;
    memset(storage, 0xa5, bytes);
    memcpy(storage + 4u, block.bytes, block.size);
    memcpy(saved, storage, bytes);
    /* Exact alias, destination after input, and destination before input. */
    CHECK(mzg2_decode_block(&profile, 1u, 0u, storage + 4u, block.size,
        storage + 4u, sizeof(output), error, sizeof(error)) == MZG2_ERROR_BOUNDS);
    CHECK(mzg2_decode_block(&profile, 1u, 0u, storage + 4u, block.size,
        storage + 8u, sizeof(output), error, sizeof(error)) == MZG2_ERROR_BOUNDS);
    CHECK(mzg2_decode_block(&profile, 1u, 0u, storage + 4u, block.size,
        storage, sizeof(output), error, sizeof(error)) == MZG2_ERROR_BOUNDS);
    CHECK(memcmp(saved, storage, bytes) == 0);
    /* An unaligned base must fail before typed model-table reads. */
    memcpy(storage + 1u, block.bytes, block.size);
    CHECK(mzg2_decode_block(&profile, 1u, 0u, storage + 1u, block.size,
        output, sizeof(output), error, sizeof(error)) == MZG2_ERROR_BOUNDS);
    profile.model_count = MZG2_MAX_MODELS + 1u;
    CHECK(!mzg2_block_validate(&profile, 1u, 0u, block.bytes, block.size,
                               &view, error, sizeof(error)));
    CHECK(!mzg2_model_entry_validate(&entry, &profile));
    mzg2_profile_tile_rules(&profile, &rules);
    CHECK(rules.model_count == 0u);
    CHECK(!mzg2_models_from_planes(&profile, payload, mzg2_fixture_plane_value,
        2u, models, &entry, error, sizeof(error)));
    profile = *mzg2_fixture_profile();
    CHECK(mzg2_models_from_planes(&profile, payload, mzg2_fixture_plane_value,
        2u, models, &entry, error, sizeof(error)));
    /* The sum wraps to 8192 in the old encoder; reject before source access. */
    planes[0] = (mzg2_plane){0u, UINT32_MAX - 1023u, 0u};
    planes[1] = (mzg2_plane){UINT32_MAX - 1023u, 9216u, 1u};
    CHECK(!mzg2_encode_block(&profile, 1u, 0u, payload, planes, 2u, models,
        &entry, &rejected, error, sizeof(error)));
    CHECK(rejected.bytes == NULL);
    CHECK(mzg2_decode_block(&profile, 1u, 0u, block.bytes, block.size, output,
        sizeof(output), error, sizeof(error)) == 0u);
    CHECK(memcmp(payload, output, sizeof(payload)) == 0);
done:
    free(storage);
    free(saved);
    mzg2_encoded_release(&block);
}

int main(void)
{
    test_arithmetic();
    test_buffers_and_profiles();
    return mzg2_test_report("test_api");
}
