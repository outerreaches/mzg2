/*
 * mzg2_vectors: write the golden vector blocks and their manifest.
 *
 * The vectors are committed, so this tool only needs running when the
 * bitstream intentionally changes -- and then the diff is the evidence that
 * it changed. `make vectors` regenerates them in place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "mzg2/mzg2.h"
#include "vectors.h"

int main(int argc, char **argv)
{
    const char *directory = argc > 1 ? argv[1] : "tests/vectors";
    char manifest_path[512];
    FILE *manifest;
    uint32_t index;
    int failures = 0;

    snprintf(manifest_path, sizeof(manifest_path), "%s/%s", directory,
             MZG2_VECTOR_MANIFEST);
    manifest = fopen(manifest_path, "wb");
    if (!manifest) {
        fprintf(stderr, "cannot write %s\n", manifest_path);
        return 1;
    }
    fprintf(manifest,
            "# MZG2 golden vectors. Columns: name, layer, expert, "
            "block_bytes,\n"
            "# tile_count, raw_tiles, payload FNV-1a, block FNV-1a.\n"
            "# Profile: the synthetic fixture in tests/fixture.h, not a "
            "model.\n");

    for (index = 0u; index < MZG2_VECTOR_COUNT; ++index) {
        const mzg2_vector *vector = &mzg2_vector_catalogue[index];
        uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
        mzg2_encoded encoded;
        char path[512];
        char error[256];
        FILE *file;

        vector->fill(payload);
        if (!mzg2_fixture_encode(payload, vector->layer, vector->expert,
                                 &encoded, error, sizeof(error))) {
            fprintf(stderr, "%s: encode failed: %s\n", vector->name, error);
            ++failures;
            continue;
        }
        mzg2_vector_path(path, sizeof(path), directory, vector->name);
        file = fopen(path, "wb");
        if (!file ||
            fwrite(encoded.bytes, 1u, encoded.size, file) != encoded.size) {
            fprintf(stderr, "%s: cannot write %s\n", vector->name, path);
            ++failures;
        }
        if (file) {
            (void)fclose(file);
        }
        fprintf(manifest, "%s %u %u %u %u %u %016llx %016llx\n", vector->name,
                vector->layer, vector->expert, encoded.size,
                encoded.tile_count, encoded.raw_tiles,
                (unsigned long long)mzg2_fixture_checksum(payload,
                                                          sizeof(payload)),
                (unsigned long long)mzg2_fixture_checksum(encoded.bytes,
                                                          encoded.size));
        printf("%-20s %8u bytes  %3u tiles  %3u raw\n", vector->name,
               encoded.size, encoded.tile_count, encoded.raw_tiles);
        mzg2_encoded_release(&encoded);
    }
    if (fclose(manifest) != 0) {
        ++failures;
    }
    return failures == 0 ? 0 : 1;
}
