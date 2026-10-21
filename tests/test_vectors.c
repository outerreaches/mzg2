/*
 * Golden vectors: decode the committed blocks and check them against
 * payloads regenerated from the catalogue, then re-encode for byte
 * identity.
 *
 * This is the regression anchor for the bitstream. If a change to the codec
 * alters a single byte of a vector, this test fails -- and the fix is either
 * to revert the change or to regenerate the vectors deliberately, with the
 * diff as the record that the format moved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fixture.h"
#include "harness.h"
#include "mzg2/mzg2.h"
#include "vectors.h"

#ifndef MZG2_VECTOR_DIR
#define MZG2_VECTOR_DIR "tests/vectors"
#endif

static void check_vector(const char *directory, const mzg2_vector *vector)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    uint8_t expected[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t decoded[MZG2_FIXTURE_OUTPUT_BYTES];
    mzg2_block_view view;
    mzg2_encoded again;
    char path[512];
    char error[256];
    uint8_t *block;
    size_t bytes = 0u;
    uint32_t flags;

    mzg2_vector_path(path, sizeof(path), directory, vector->name);
    block = mzg2_test_read_file(path, &bytes);
    CHECK_MSG(block != NULL, "cannot read %s", path);
    if (!block) {
        return;
    }
    printf("  %s: %zu bytes\n", vector->name, bytes);
    CHECK(bytes % MZG2_ALIGNMENT == 0u);

    CHECK_MSG(mzg2_block_validate(profile, vector->layer, vector->expert,
                                  block, (uint32_t)bytes, &view, error,
                                  sizeof(error)),
              "%s validate: %s", vector->name, error);

    vector->fill(expected);
    memset(decoded, 0xa5, sizeof(decoded));
    flags = mzg2_decode_block(profile, vector->layer, vector->expert, block,
                              (uint32_t)bytes, decoded, sizeof(decoded),
                              error, sizeof(error));
    CHECK_MSG(flags == 0u, "%s decode flags %u: %s", vector->name, flags,
              error);
    CHECK_MSG(memcmp(decoded, expected, sizeof(expected)) == 0,
              "%s payload differs from the catalogue", vector->name);

    /* encode(decode(vector)) == vector */
    if (mzg2_fixture_encode(expected, vector->layer, vector->expert, &again,
                            error, sizeof(error))) {
        CHECK_MSG(again.size == (uint32_t)bytes, "%s re-encodes to %u bytes",
                  vector->name, again.size);
        if (again.size == (uint32_t)bytes) {
            CHECK_MSG(memcmp(again.bytes, block, bytes) == 0,
                      "%s re-encode differs", vector->name);
        }
        mzg2_encoded_release(&again);
    } else {
        CHECK_MSG(false, "%s re-encode failed: %s", vector->name, error);
    }
    free(block);
}

/* The manifest is a human-readable record; check the numbers it states. */
static void check_manifest(const char *directory)
{
    char path[512];
    FILE *file;
    char line[512];
    uint32_t described = 0u;

    snprintf(path, sizeof(path), "%s/%s", directory, MZG2_VECTOR_MANIFEST);
    file = fopen(path, "rb");
    CHECK_MSG(file != NULL, "cannot read %s", path);
    if (!file) {
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        char name[128];
        unsigned layer;
        unsigned expert;
        unsigned block_bytes;
        unsigned tile_count;
        unsigned raw_tiles;
        unsigned long long payload_hash;
        unsigned long long block_hash;
        uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
        uint8_t *block;
        size_t bytes = 0u;
        char block_path[512];
        uint32_t index;
        const mzg2_vector *vector = NULL;

        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        if (sscanf(line, "%127s %u %u %u %u %u %llx %llx", name, &layer,
                   &expert, &block_bytes, &tile_count, &raw_tiles,
                   &payload_hash, &block_hash) != 8) {
            CHECK_MSG(false, "unparsable manifest line: %s", line);
            continue;
        }
        for (index = 0u; index < MZG2_VECTOR_COUNT; ++index) {
            if (strcmp(mzg2_vector_catalogue[index].name, name) == 0) {
                vector = &mzg2_vector_catalogue[index];
                break;
            }
        }
        CHECK_MSG(vector != NULL, "manifest names unknown vector %s", name);
        if (!vector) {
            continue;
        }
        ++described;
        CHECK(vector->layer == layer);
        CHECK(vector->expert == expert);
        CHECK(tile_count == mzg2_profile_tile_count(mzg2_fixture_profile()));

        vector->fill(payload);
        CHECK_MSG(mzg2_fixture_checksum(payload, sizeof(payload)) ==
                      payload_hash,
                  "%s payload checksum", name);
        mzg2_vector_path(block_path, sizeof(block_path), directory, name);
        block = mzg2_test_read_file(block_path, &bytes);
        if (block) {
            CHECK_MSG((unsigned)bytes == block_bytes, "%s block bytes", name);
            CHECK_MSG(mzg2_fixture_checksum(block, bytes) == block_hash,
                      "%s block checksum", name);
            free(block);
        }
    }
    (void)fclose(file);
    CHECK_MSG(described == MZG2_VECTOR_COUNT,
              "manifest describes %u of %u vectors", described,
              MZG2_VECTOR_COUNT);
}

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : MZG2_VECTOR_DIR;
    uint32_t index;

    printf("golden vectors from %s\n", directory);
    for (index = 0u; index < MZG2_VECTOR_COUNT; ++index) {
        check_vector(directory, &mzg2_vector_catalogue[index]);
    }
    check_manifest(directory);
    return mzg2_test_report("test_vectors");
}
