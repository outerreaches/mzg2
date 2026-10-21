/* Profile consistency, derived geometry, and model build/validate rules. */
#include "fixture.h"
#include "harness.h"
#include "mzg2/adapters/kimi_k3.h"
#include "mzg2/mzg2.h"

static void test_abi_sizes(void)
{
    printf("ABI sizes\n");
    CHECK(sizeof(mzg2_file_header) == 124u);
    CHECK(sizeof(mzg2_index_entry) == 16u);
    CHECK(sizeof(mzg2_block_header) == 64u);
    CHECK(sizeof(mzg2_tile_descriptor) == 32u);
    CHECK(sizeof(mzg2_model) == 4248u);
    CHECK(sizeof(mzg2_model_entry) == 8496u);
    /* The block header's model_offset rule is "a multiple of 4"; that is
     * only meaningful if the model entry's alignment really is 4. */
    CHECK(_Alignof(mzg2_model_entry) == 4u);
}

static void test_profiles(void)
{
    const mzg2_profile *k3 = mzg2_kimi_k3_profile();
    const mzg2_profile *fixture = mzg2_fixture_profile();
    mzg2_profile broken;
    mzg2_tile_rules rules;
    char error[256];

    printf("profiles\n");
    CHECK_MSG(mzg2_profile_check(k3, error, sizeof(error)), "K3: %s", error);
    CHECK_MSG(mzg2_profile_check(fixture, error, sizeof(error)), "fixture: %s",
              error);
    CHECK(mzg2_profile_check(mzg2_kimi_k3_profile_unbound(), error,
                             sizeof(error)));

    /* K3 geometry: 17,547,264 / 16,384 = 1071 full tiles. */
    CHECK(mzg2_profile_tile_count(k3) == 1071u);
    CHECK(mzg2_profile_tile_output_bytes(k3, 0u) == 16384u);
    CHECK(mzg2_profile_tile_output_bytes(k3, 1070u) == 16384u);
    CHECK(mzg2_profile_tile_output_bytes(k3, 1071u) == 0u);
    CHECK(mzg2_profile_tile_count(fixture) == 8u);

    mzg2_profile_tile_rules(k3, &rules);
    CHECK(rules.tile_count == 1071u);
    CHECK(rules.output_bytes == MZG2_K3_EXPERT_BYTES);
    CHECK(rules.model_count == 2u);
    CHECK(rules.symbol_bits[MZG2_K3_MODEL_PACKED] == 4u);
    CHECK(rules.symbol_bits[MZG2_K3_MODEL_SCALE] == 8u);

    /* Each rejection below must be refused for its own reason. */
    broken = *fixture;
    broken.tile_bytes = 100u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.output_bytes = 0u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.output_bytes = MZG2_FIXTURE_OUTPUT_BYTES + 64u; /* not a granule */
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.model_count = MZG2_MAX_MODELS + 1u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.model[0].symbol_bits = 5u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.magic[0] = '\0';
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.sidecar_pattern = NULL;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.max_layers = 0u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
    broken = *fixture;
    broken.experts_per_layer = 0u;
    CHECK(!mzg2_profile_check(&broken, error, sizeof(error)));
}

static void test_model_build(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    const mzg2_model_spec *nibble = &profile->model[0];
    const mzg2_model_spec *byte_plane = &profile->model[1];
    uint64_t counts[256];
    mzg2_built_model first;
    mzg2_built_model second;
    uint32_t symbol;
    uint32_t total = 0u;

    printf("model build\n");
    memset(counts, 0, sizeof(counts));
    counts[0] = 700u;
    counts[1] = 200u;
    counts[2] = 90u;
    counts[8] = 500u;  /* removed by the alphabet: must not get a symbol */
    counts[15] = 10u;
    counts[200] = 50u; /* outside a 4-bit alphabet */

    CHECK(mzg2_model_build(counts, nibble, &first));
    CHECK(first.table.symbol_count == 4u);
    CHECK(first.table.values[0] == 0u);
    CHECK(first.table.values[1] == 1u);
    CHECK(first.table.values[2] == 2u);
    CHECK(first.table.values[3] == 15u);
    CHECK(first.symbol_for_value[8] == 0xffu);
    CHECK(first.symbol_for_value[200] == 0xffu);
    CHECK(first.symbol_for_value[15] == 3u);
    for (symbol = 0u; symbol < first.table.symbol_count; ++symbol) {
        CHECK(first.table.frequency[symbol] != 0u);
        CHECK(first.table.cumulative[symbol] == total);
        total += first.table.frequency[symbol];
    }
    CHECK(total == MZG2_MODEL_SLOTS);
    CHECK(mzg2_model_validate(&first.table, nibble));

    /* Determinism: the table bytes are part of a stored block. */
    CHECK(mzg2_model_build(counts, nibble, &second));
    CHECK(memcmp(&first.table, &second.table, sizeof(first.table)) == 0);

    /* The same counts under the 8-bit spec admit values the 4-bit one
     * rejects, including 8 and 200. */
    CHECK(mzg2_model_build(counts, byte_plane, &second));
    CHECK(second.table.symbol_count == 6u);
    CHECK(second.symbol_for_value[8] != 0xffu);
    CHECK(second.symbol_for_value[200] != 0xffu);

    /* A rare symbol still gets a nonzero frequency. */
    memset(counts, 0, sizeof(counts));
    counts[0] = 1000000u;
    counts[1] = 1u;
    CHECK(mzg2_model_build(counts, nibble, &first));
    CHECK(first.table.symbol_count == 2u);
    CHECK(first.table.frequency[1] >= 1u);
    CHECK(first.table.frequency[0] + first.table.frequency[1] ==
          MZG2_MODEL_SLOTS);
    CHECK(mzg2_model_validate(&first.table, nibble));

    /* Rejections. */
    memset(counts, 0, sizeof(counts));
    CHECK(!mzg2_model_build(counts, nibble, &first)); /* nothing observed */
    memset(counts, 0, sizeof(counts));
    for (symbol = 0u; symbol < 16u; ++symbol) {
        counts[symbol] = 10u + symbol;
    }
    for (symbol = 0u; symbol < 32u; ++symbol) {
        counts[100u + symbol] = 5u;
    }
    /* 4-bit: 15 permitted values observed, which is exactly the limit. */
    CHECK(mzg2_model_build(counts, nibble, &first));
    CHECK(first.table.symbol_count == 15u);
    /* 8-bit: 48 distinct values is past the format's 15 symbols. */
    CHECK(!mzg2_model_build(counts, byte_plane, &first));
}

static void test_model_validate_rejects_tampering(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    const mzg2_model_spec *nibble = &profile->model[0];
    uint64_t counts[256];
    mzg2_built_model good;
    mzg2_model model;

    printf("model validation\n");
    memset(counts, 0, sizeof(counts));
    counts[0] = 500u;
    counts[1] = 300u;
    counts[2] = 200u;
    CHECK(mzg2_model_build(counts, nibble, &good));
    CHECK(mzg2_model_validate(&good.table, nibble));

    /* Frequencies must sum to exactly MZG2_MODEL_SLOTS. */
    model = good.table;
    model.frequency[0] += 1u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* Cumulative offsets must follow the frequencies. */
    model = good.table;
    model.cumulative[1] += 1u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* A zero frequency is not a symbol. */
    model = good.table;
    model.frequency[2] = 0u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* Values must be distinct. */
    model = good.table;
    model.values[1] = model.values[0];
    CHECK(!mzg2_model_validate(&model, nibble));

    /* Values must be inside the plane's alphabet. */
    model = good.table;
    model.values[1] = 8u;
    CHECK(!mzg2_model_validate(&model, nibble));
    model = good.table;
    model.values[1] = 200u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* The decode table must agree with the frequency layout. */
    model = good.table;
    model.decode[0] = 2u;
    CHECK(!mzg2_model_validate(&model, nibble));
    model = good.table;
    model.decode[MZG2_MODEL_SLOTS - 1u] = 0u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* Symbol count bounds. */
    model = good.table;
    model.symbol_count = 0u;
    CHECK(!mzg2_model_validate(&model, nibble));
    model = good.table;
    model.symbol_count = MZG2_MAX_SYMBOLS + 1u;
    CHECK(!mzg2_model_validate(&model, nibble));

    /* Padding is unspecified on read and must not affect acceptance. */
    model = good.table;
    memset(model.padding, 0xa5, sizeof(model.padding));
    CHECK(mzg2_model_validate(&model, nibble));
}

static void test_k3_adapter(void)
{
    const mzg2_plane *planes;
    const char *const *suffixes;
    uint32_t count = 0u;
    uint32_t index;
    uint32_t expected = 0u;
    uint8_t packed[4];
    uint64_t changed;

    printf("K3 adapter\n");
    planes = mzg2_kimi_k3_planes(&count);
    CHECK(count == MZG2_K3_PLANES);
    for (index = 0u; index < count; ++index) {
        CHECK(planes[index].offset == expected);
        CHECK(planes[index].bytes % MZG2_K3_TILE_BYTES == 0u);
        expected += planes[index].bytes;
    }
    CHECK(expected == MZG2_K3_EXPERT_BYTES);
    suffixes = mzg2_kimi_k3_plane_suffixes(&count);
    CHECK(count == MZG2_K3_PLANES);
    CHECK(strcmp(suffixes[0], "w1.weight_packed") == 0);
    CHECK(strcmp(suffixes[5], "w3.weight_scale") == 0);

    /* The zero-gauge transform folds nibble 8 onto 0 and nothing else. */
    packed[0] = 0x88u;
    packed[1] = 0x18u;
    packed[2] = 0x81u;
    packed[3] = 0x77u;
    changed = mzg2_kimi_k3_canonicalize_packed(packed, sizeof(packed));
    CHECK(changed == 4u);
    CHECK(packed[0] == 0x00u);
    CHECK(packed[1] == 0x10u);
    CHECK(packed[2] == 0x01u);
    CHECK(packed[3] == 0x77u);
    /* Idempotent. */
    CHECK(mzg2_kimi_k3_canonicalize_packed(packed, sizeof(packed)) == 0u);
}

int main(void)
{
    test_abi_sizes();
    test_profiles();
    test_model_build();
    test_model_validate_rejects_tampering();
    test_k3_adapter();
    return mzg2_test_report("test_format");
}
