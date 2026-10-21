/*
 * A sidecar writer for tests and golden vectors.
 *
 * The core is a reader: an engine opens a store, never writes one. Building
 * a store is an adapter/tool concern (for Kimi K3 it lives in the Moonshine
 * transcoder), so this writer stays in the test layer until a public
 * transcode tool lands -- see docs/roadmap.md.
 */
#ifndef MZG2_TESTS_SIDECAR_H
#define MZG2_TESTS_SIDECAR_H

#include <stdio.h>

#include "fixture.h"
#include "harness.h"
#include "mzg2/mzg2.h"

static inline uint64_t mzg2_test_align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1u) / alignment * alignment;
}

/*
 * Write a fixture sidecar holding `count` experts starting at `first`.
 * `manifest` is either NULL (zero digest) or 32 bytes.
 */
static inline bool mzg2_test_write_sidecar(const char *path, uint32_t layer,
                                           uint32_t first, uint32_t count,
                                           const uint8_t *manifest,
                                           uint64_t seed)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    mzg2_file_header header;
    mzg2_index_entry *index;
    uint8_t padding[MZG2_ALIGNMENT];
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    FILE *file;
    uint64_t cursor;
    uint32_t expert;
    bool ok = true;

    index = (mzg2_index_entry *)calloc(count, sizeof(*index));
    if (!index) {
        return false;
    }
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, profile->magic, sizeof(header.magic));
    header.version = MZG2_VERSION;
    header.header_bytes = MZG2_FILE_HEADER_BYTES;
    header.alignment = MZG2_ALIGNMENT;
    header.layer = layer;
    header.expert_count = count;
    header.index_entry_bytes = sizeof(mzg2_index_entry);
    header.model_entry_bytes = 0u;
    header.tile_bytes = profile->tile_bytes;
    header.flags = 0u;
    header.index_offset = MZG2_FILE_HEADER_BYTES;
    header.index_bytes = (uint64_t)count * sizeof(mzg2_index_entry);
    header.data_offset = mzg2_test_align_up(
        header.index_offset + header.index_bytes, MZG2_ALIGNMENT);
    if (manifest) {
        memcpy(header.source_manifest_sha256, manifest,
               sizeof(header.source_manifest_sha256));
    }

    file = fopen(path, "w+b");
    if (!file) {
        free(index);
        return false;
    }
    /* Reserve the header and index regions; they are written at the end,
     * once max_block_bytes and the offsets are known. */
    memset(padding, 0, sizeof(padding));
    cursor = 0u;
    while (cursor < header.data_offset) {
        const size_t chunk = (size_t)(header.data_offset - cursor) <
                                     sizeof(padding)
                                 ? (size_t)(header.data_offset - cursor)
                                 : sizeof(padding);

        if (fwrite(padding, 1u, chunk, file) != chunk) {
            ok = false;
            break;
        }
        cursor += chunk;
    }

    for (expert = 0u; ok && expert < count; ++expert) {
        mzg2_encoded encoded;
        char error[256];
        uint64_t tail;

        mzg2_fixture_fill(payload, seed + layer * 1000u + first + expert);
        if (!mzg2_fixture_encode(payload, layer, first + expert, &encoded,
                                 error, sizeof(error))) {
            ok = false;
            break;
        }
        index[expert].layer = (uint16_t)layer;
        index[expert].expert = (uint16_t)(first + expert);
        index[expert].block_bytes = encoded.size;
        index[expert].block_offset = cursor;
        if (fwrite(encoded.bytes, 1u, encoded.size, file) != encoded.size) {
            ok = false;
        }
        cursor += encoded.size;
        if (encoded.size > header.max_block_bytes) {
            header.max_block_bytes = encoded.size;
        }
        /* Blocks are already a multiple of the alignment, but keep the
         * invariant explicit. */
        tail = mzg2_test_align_up(cursor, MZG2_ALIGNMENT) - cursor;
        while (ok && tail != 0u) {
            const size_t chunk =
                (size_t)tail < sizeof(padding) ? (size_t)tail
                                               : sizeof(padding);

            if (fwrite(padding, 1u, chunk, file) != chunk) {
                ok = false;
            }
            cursor += chunk;
            tail -= chunk;
        }
        mzg2_encoded_release(&encoded);
    }

    if (ok) {
        ok = fseek(file, 0, SEEK_SET) == 0 &&
             fwrite(&header, 1u, sizeof(header), file) == sizeof(header) &&
             fseek(file, (long)header.index_offset, SEEK_SET) == 0 &&
             fwrite(index, sizeof(*index), count, file) == count;
    }
    if (fclose(file) != 0) {
        ok = false;
    }
    free(index);
    return ok;
}

#endif /* MZG2_TESTS_SIDECAR_H */
