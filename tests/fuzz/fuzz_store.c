/* Bound every allocation by a synthetic profile; fuzz file headers/indexes. */
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include "fixture.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[] = "/tmp/mzg2-fuzz-store-XXXXXX";
    char error[256];
    mzg2_store *store = NULL;
    uint8_t block[65536];
    uint8_t output[MZG2_FIXTURE_OUTPUT_BYTES];
    uint32_t layer, expert;
    size_t written = 0;
    int fd;

    if (size > sizeof(block)) return 0;
    fd = mkstemp(path);
    if (fd < 0) abort();
    while (written < size) {
        ssize_t n = write(fd, data + written, size - written);
        if (n <= 0) abort();
        written += (size_t)n;
    }
    if (close(fd) != 0) abort();
    if (mzg2_store_open(mzg2_fixture_profile(), path, &store,
                        error, sizeof(error))) {
        for (layer = 1; layer <= MZG2_FIXTURE_LAYERS; ++layer) {
            for (expert = 0; expert < MZG2_FIXTURE_EXPERTS; ++expert) {
                uint32_t bytes;
                if (mzg2_store_read_block(store, layer, expert, block,
                        sizeof(block), &bytes, error, sizeof(error)))
                    (void)mzg2_decode_block(mzg2_fixture_profile(), layer,
                        expert, block, bytes, output, sizeof(output), error,
                        sizeof(error));
            }
        }
        mzg2_store_destroy(store);
    } else if (store != NULL) abort();
    if (unlink(path) != 0) abort();
    return 0;
}
