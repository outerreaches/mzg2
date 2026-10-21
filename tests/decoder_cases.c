/* C fixture bridge: keeps the HIP runner independent of C fixture syntax. */
#include "decoder_cases.h"
#include "malformed_cases.h"
#include "vectors.h"
#include "mzg2/adapters/kimi_k3.h"

static void golden_cases(mzg2_test_decode_fn decode)
{
    uint32_t i;
    for (i = 0; i < MZG2_VECTOR_COUNT; ++i) {
        const mzg2_vector *vector = &mzg2_vector_catalogue[i];
        uint8_t expected[MZG2_FIXTURE_OUTPUT_BYTES];
        uint8_t cpu[MZG2_FIXTURE_OUTPUT_BYTES], output[MZG2_FIXTURE_OUTPUT_BYTES];
        char path[256], error[256];
        size_t size;
        uint8_t *block;
        mzg2_vector_path(path, sizeof(path), "tests/vectors", vector->name);
        block = mzg2_test_read_file(path, &size);
        CHECK(block != NULL);
        if (!block) continue;
        vector->fill(expected);
        CHECK(mzg2_decode_block(mzg2_fixture_profile(), vector->layer,
            vector->expert, block, (uint32_t)size, cpu, sizeof(cpu), error,
            sizeof(error)) == 0u);
        CHECK(memcmp(cpu, expected, sizeof(cpu)) == 0);
        CHECK(decode(mzg2_fixture_profile(), vector->layer, vector->expert,
            block, (uint32_t)size, output, sizeof(output), error, sizeof(error)) == 0u);
        CHECK(memcmp(cpu, output, sizeof(cpu)) == 0);
        printf("golden: %s\n", vector->name);
        free(block);
    }
}

static void real_cases(mzg2_test_decode_fn decode, const char *path,
                        uint32_t layer, uint32_t first, uint32_t experts)
{
    const mzg2_profile *profile = mzg2_kimi_k3_profile();
    mzg2_store *store = NULL;
    uint8_t *block = NULL, *cpu = NULL, *output = NULL;
    uint32_t capacity, i;
    char error[256];
    CHECK(mzg2_store_open(profile, path, &store, error, sizeof(error)));
    if (!store) return;
    capacity = mzg2_store_max_block_bytes(store);
    CHECK(capacity <= 64u * 1024u * 1024u);
    if (capacity > 64u * 1024u * 1024u) goto done;
    block = (uint8_t *)malloc(capacity);
    cpu = (uint8_t *)malloc(profile->output_bytes);
    output = (uint8_t *)malloc(profile->output_bytes);
    CHECK(block && cpu && output);
    if (!block || !cpu || !output) goto done;
    for (i = 0; i < experts; ++i) {
        uint32_t size = 0, flags;
        const uint32_t expert = first + i;
        const bool read = mzg2_store_read_block(store, layer, expert, block,
            capacity, &size, error, sizeof(error));
        CHECK(read);
        if (!read) continue;
        flags = mzg2_decode_block(profile, layer, expert, block, size, cpu,
            profile->output_bytes, error, sizeof(error));
        CHECK(flags == 0u);
        if (flags) continue;
        flags = decode(profile, layer, expert, block, size, output,
            profile->output_bytes, error, sizeof(error));
        CHECK(flags == 0u);
        if (!flags) CHECK(memcmp(cpu, output, profile->output_bytes) == 0);
        printf("source-bound real block: layer=%u expert=%u bytes=%u\n",
               layer, expert, size);
    }
done:
    free(block);
    free(cpu);
    free(output);
    mzg2_store_destroy(store);
}

int mzg2_run_decoder_cases(mzg2_test_decode_fn decode, const char *store_path,
                           uint32_t layer, uint32_t first, uint32_t experts)
{
    mzg2_test_checks = 0;
    mzg2_test_failures = 0;
    golden_cases(decode);
    (void)mzg2_run_malformed(decode);
    if (store_path) real_cases(decode, store_path, layer, first, experts);
    else printf("real-store parity: NOT RUN (no --store supplied)\n");
    return mzg2_test_report("decoder qualification cases");
}
