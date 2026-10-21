/* Deterministic seeds from the same synthetic catalogue as the unit tests. */
#include "vectors.h"
#include "sidecar.h"

static void write_seed(const char *root, const char *kind, const char *name,
                        const void *data, size_t bytes)
{
    char path[4096];
    FILE *file;
    if (snprintf(path, sizeof(path), "%s/%s/%s", root, kind, name) >=
        (int)sizeof(path)) exit(1);
    file = fopen(path, "wb");
    if (!file || fwrite(data, 1, bytes, file) != bytes) exit(1);
    if (fclose(file) != 0) exit(1);
}

int main(int argc, char **argv)
{
    uint32_t i;
    char path[4096];
    if (argc != 2) return 2;
    for (i = 0; i < MZG2_VECTOR_COUNT; ++i) {
        uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
        mzg2_encoded encoded;
        mzg2_block_header header;
        char error[256];
        const mzg2_vector *vector = &mzg2_vector_catalogue[i];
        vector->fill(payload);
        if (!mzg2_fixture_encode(payload, vector->layer, vector->expert,
                                 &encoded, error, sizeof(error))) return 1;
        write_seed(argv[1], "block", vector->name, encoded.bytes, encoded.size);
        write_seed(argv[1], "roundtrip", vector->name, payload, sizeof(payload));
        memcpy(&header, encoded.bytes, sizeof(header));
        write_seed(argv[1], "model", vector->name,
                   encoded.bytes + header.model_offset, sizeof(mzg2_model));
        mzg2_encoded_release(&encoded);
    }
    if (snprintf(path, sizeof(path), "%s/store/fixture-sidecar", argv[1]) >=
        (int)sizeof(path)) return 1;
    return mzg2_test_write_sidecar(path, 1u, 0u, 2u, NULL,
                                   MZG2_FIXTURE_SEED) ? 0 : 1;
}
