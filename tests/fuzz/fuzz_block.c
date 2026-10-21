/* Mutate complete synthetic blocks; never open model weights. */
#include "fixture.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    mzg2_block_header header;
    mzg2_block_view view;
    uint8_t output[MZG2_FIXTURE_OUTPUT_BYTES + 64u];
    char error[256];
    uint8_t *block;
    uint32_t flags;
    bool valid;
    size_t i;

    if (size < sizeof(header) || size > 65536u) return 0;
    memcpy(&header, data, sizeof(header));
    block = (uint8_t *)malloc(size);
    if (!block) abort();
    memcpy(block, data, size);
    memset(output, 0xa5, sizeof(output));
    valid = mzg2_block_validate(mzg2_fixture_profile(), header.layer,
        header.expert, block, (uint32_t)size, &view, error, sizeof(error));
    flags = mzg2_decode_block(mzg2_fixture_profile(), header.layer,
        header.expert, block, (uint32_t)size, output + 32u,
        MZG2_FIXTURE_OUTPUT_BYTES, error, sizeof(error));
    if (!valid && flags != MZG2_ERROR_BOUNDS) abort();
    for (i = 0; i < 32u; ++i) {
        if (output[i] != 0xa5 ||
            output[32u + MZG2_FIXTURE_OUTPUT_BYTES + i] != 0xa5) abort();
    }
    if (!valid) {
        for (i = 32u; i < 32u + MZG2_FIXTURE_OUTPUT_BYTES; ++i)
            if (output[i] != 0xa5) abort();
    }
    free(block);
    return 0;
}
