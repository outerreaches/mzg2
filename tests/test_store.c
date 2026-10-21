/*
 * Store opening: a synthetic store on disk, the span/read path, and the
 * fail-closed rules -- magic, version, index bounds, duplicate experts,
 * source binding and directory completeness.
 *
 * Everything here is built by the test. No model weights, no fixed paths:
 * the store lives in a temporary directory removed on the way out.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fixture.h"
#include "harness.h"
#include "mzg2/mzg2.h"
#include "sidecar.h"

static const uint8_t test_manifest[32] = {
    0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
    0x99u, 0xaau, 0xbbu, 0xccu, 0xddu, 0xeeu, 0xffu, 0x00u,
    0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u,
    0x09u, 0x0au, 0x0bu, 0x0cu, 0x0du, 0x0eu, 0x0fu, 0x10u
};

static char root[256];

static void sidecar_path(char *buffer, size_t size, uint32_t layer)
{
    char name[64];

    snprintf(name, sizeof(name), "layer-%03u.tst", layer);
    snprintf(buffer, size, "%s/%s", root, name);
}

static void test_single_file(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    char path[512];
    mzg2_store *store = NULL;
    mzg2_span span;
    char error[512];
    uint8_t *block;
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
    uint8_t expected[MZG2_FIXTURE_OUTPUT_BYTES];
    uint32_t block_bytes = 0u;

    printf("single sidecar\n");
    sidecar_path(path, sizeof(path), 1u);
    CHECK(mzg2_test_write_sidecar(path, 1u, 0u, MZG2_FIXTURE_EXPERTS, NULL,
                                  MZG2_FIXTURE_SEED));
    CHECK_MSG(mzg2_store_open(profile, path, &store, error, sizeof(error)),
              "open: %s", error);
    if (!store) {
        return;
    }
    CHECK(mzg2_store_present_count(store) == MZG2_FIXTURE_EXPERTS);
    CHECK(mzg2_store_max_block_bytes(store) % MZG2_ALIGNMENT == 0u);
    CHECK(mzg2_store_span(store, 1u, 0u, &span));
    CHECK(span.direct_fd >= 0);
    CHECK(span.offset % MZG2_ALIGNMENT == 0u);
    CHECK(span.bytes % MZG2_ALIGNMENT == 0u);
    /* Absent identities are reported, not guessed. */
    CHECK(!mzg2_store_span(store, 2u, 0u, &span));
    CHECK(!mzg2_store_span(store, 1u, MZG2_FIXTURE_EXPERTS, &span));
    CHECK(!mzg2_store_span(store, 0u, 0u, &span));

    block = (uint8_t *)malloc(mzg2_store_max_block_bytes(store));
    if (!block) {
        mzg2_store_destroy(store);
        return;
    }
    CHECK_MSG(mzg2_store_read_block(store, 1u, 2u, block,
                                    mzg2_store_max_block_bytes(store),
                                    &block_bytes, error, sizeof(error)),
              "read: %s", error);
    CHECK(mzg2_decode_block(profile, 1u, 2u, block, block_bytes, payload,
                            sizeof(payload), error, sizeof(error)) == 0u);
    mzg2_fixture_fill(expected, MZG2_FIXTURE_SEED + 1000u + 2u);
    CHECK(memcmp(payload, expected, sizeof(expected)) == 0);
    /* A buffer shorter than the block is refused. */
    CHECK(!mzg2_store_read_block(store, 1u, 2u, block, 4096u, &block_bytes,
                                 error, sizeof(error)));
    free(block);
    mzg2_store_destroy(store);
}

static void test_directory(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    mzg2_profile complete;
    mzg2_store *store = NULL;
    char path[512];
    char error[512];

    printf("directory store\n");
    sidecar_path(path, sizeof(path), 2u);
    CHECK(mzg2_test_write_sidecar(path, 2u, 0u, MZG2_FIXTURE_EXPERTS, NULL,
                                  MZG2_FIXTURE_SEED));
    CHECK_MSG(mzg2_store_open(profile, root, &store, error, sizeof(error)),
              "open: %s", error);
    if (store) {
        CHECK(mzg2_store_present_count(store) ==
              (uint64_t)MZG2_FIXTURE_LAYERS * MZG2_FIXTURE_EXPERTS);
        mzg2_store_destroy(store);
        store = NULL;
    }

    /* A store missing one expert fails the completeness rule. */
    complete = *profile;
    complete.require_complete_directory = true;
    CHECK(mzg2_store_open(&complete, root, &store, error, sizeof(error)));
    mzg2_store_destroy(store);
    store = NULL;
    sidecar_path(path, sizeof(path), 2u);
    CHECK(mzg2_test_write_sidecar(path, 2u, 0u, MZG2_FIXTURE_EXPERTS - 1u,
                                  NULL, MZG2_FIXTURE_SEED));
    CHECK(!mzg2_store_open(&complete, root, &store, error, sizeof(error)));
    CHECK(store == NULL);
    /* Restore the complete sidecar for the remaining cases. */
    CHECK(mzg2_test_write_sidecar(path, 2u, 0u, MZG2_FIXTURE_EXPERTS, NULL,
                                  MZG2_FIXTURE_SEED));
}

static void test_rejections(void)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    mzg2_profile bound;
    mzg2_store *store = NULL;
    char path[512];
    char error[512];
    uint8_t *image;
    size_t bytes = 0u;
    FILE *file;

    printf("store rejections\n");
    sidecar_path(path, sizeof(path), 1u);

    /* Relative paths are refused: a store is named absolutely or not at
     * all. */
    CHECK(!mzg2_store_open(profile, "layer-001.tst", &store, error,
                           sizeof(error)));
    CHECK(!mzg2_store_open(profile, "/nonexistent/mzg2-store", &store, error,
                           sizeof(error)));

    /* Source binding: the fixture store carries a zero digest, so a profile
     * pinned to a real digest must reject it. */
    bound = *profile;
    bound.source_manifest_sha256 = test_manifest;
    CHECK(!mzg2_store_open(&bound, path, &store, error, sizeof(error)));
    CHECK(store == NULL);
    /* And a store written with that digest is accepted. */
    CHECK(mzg2_test_write_sidecar(path, 1u, 0u, MZG2_FIXTURE_EXPERTS,
                                  test_manifest, MZG2_FIXTURE_SEED));
    CHECK_MSG(mzg2_store_open(&bound, path, &store, error, sizeof(error)),
              "bound open: %s", error);
    mzg2_store_destroy(store);
    store = NULL;

    image = mzg2_test_read_file(path, &bytes);
    CHECK(image != NULL && bytes > MZG2_FILE_HEADER_BYTES);
    if (!image) {
        return;
    }

#define REWRITE_AND_EXPECT_REJECTION(label, statements)                       \
    do {                                                                      \
        uint8_t *copy = (uint8_t *)malloc(bytes);                             \
        mzg2_file_header *header;                                             \
        mzg2_index_entry *index;                                              \
                                                                              \
        if (!copy) {                                                          \
            break;                                                            \
        }                                                                     \
        memcpy(copy, image, bytes);                                           \
        header = (mzg2_file_header *)copy;                                    \
        index = (mzg2_index_entry *)(copy + MZG2_FILE_HEADER_BYTES);          \
        (void)index;                                                          \
        (void)header;                                                         \
        statements;                                                           \
        file = fopen(path, "wb");                                             \
        if (file) {                                                           \
            (void)fwrite(copy, 1u, bytes, file);                              \
            (void)fclose(file);                                               \
        }                                                                     \
        CHECK_MSG(!mzg2_store_open(&bound, path, &store, error,               \
                                   sizeof(error)),                            \
                  "%s was accepted", label);                                  \
        mzg2_store_destroy(store);                                            \
        store = NULL;                                                         \
        free(copy);                                                           \
    } while (0)

    REWRITE_AND_EXPECT_REJECTION("file magic", header->magic[0] = 'X');
    REWRITE_AND_EXPECT_REJECTION("version", header->version = 3u);
    REWRITE_AND_EXPECT_REJECTION("header_bytes", header->header_bytes = 512u);
    REWRITE_AND_EXPECT_REJECTION("alignment", header->alignment = 512u);
    REWRITE_AND_EXPECT_REJECTION("tile_bytes", header->tile_bytes = 4096u);
    REWRITE_AND_EXPECT_REJECTION("layer zero", header->layer = 0u);
    REWRITE_AND_EXPECT_REJECTION("layer past the profile",
                                 header->layer = MZG2_FIXTURE_LAYERS + 1u);
    REWRITE_AND_EXPECT_REJECTION("expert_count past the profile",
                                 header->expert_count =
                                     MZG2_FIXTURE_EXPERTS + 1u);
    REWRITE_AND_EXPECT_REJECTION("index_entry_bytes",
                                 header->index_entry_bytes = 8u);
    REWRITE_AND_EXPECT_REJECTION("model region declared",
                                 header->model_bytes = 64u);
    REWRITE_AND_EXPECT_REJECTION("index_offset", header->index_offset = 8192u);
    REWRITE_AND_EXPECT_REJECTION("index_bytes", header->index_bytes = 16u);
    REWRITE_AND_EXPECT_REJECTION("data_offset inside the index",
                                 header->data_offset = 4096u);
    REWRITE_AND_EXPECT_REJECTION("data_offset misaligned",
                                 header->data_offset += 1u);
    REWRITE_AND_EXPECT_REJECTION("data_offset past the file",
                                 header->data_offset = (uint64_t)bytes * 2u);
    REWRITE_AND_EXPECT_REJECTION("max_block_bytes zero",
                                 header->max_block_bytes = 0u);
    REWRITE_AND_EXPECT_REJECTION("max_block_bytes misaligned",
                                 header->max_block_bytes += 1u);
    REWRITE_AND_EXPECT_REJECTION("nonzero flags", header->flags = 2u);
    REWRITE_AND_EXPECT_REJECTION("source manifest",
                                 header->source_manifest_sha256[0] ^= 0xffu);
    REWRITE_AND_EXPECT_REJECTION("index layer disagrees with the header",
                                 index[1].layer = 2u);
    REWRITE_AND_EXPECT_REJECTION("index expert past the profile",
                                 index[1].expert = MZG2_FIXTURE_EXPERTS);
    REWRITE_AND_EXPECT_REJECTION("duplicate expert",
                                 index[1].expert = index[0].expert);
    REWRITE_AND_EXPECT_REJECTION("block_bytes zero",
                                 index[0].block_bytes = 0u);
    REWRITE_AND_EXPECT_REJECTION("block_bytes misaligned",
                                 index[0].block_bytes += 1u);
    REWRITE_AND_EXPECT_REJECTION("block_bytes past max_block_bytes",
                                 index[0].block_bytes =
                                     (uint32_t)header->max_block_bytes +
                                     MZG2_ALIGNMENT);
    REWRITE_AND_EXPECT_REJECTION("block_offset below data_offset",
                                 index[0].block_offset = 4096u);
    REWRITE_AND_EXPECT_REJECTION("block_offset misaligned",
                                 index[0].block_offset += 1u);
    REWRITE_AND_EXPECT_REJECTION("block extends past the file",
                                 index[0].block_offset =
                                     (uint64_t)bytes - MZG2_ALIGNMENT);
    REWRITE_AND_EXPECT_REJECTION("truncated file", bytes -= MZG2_ALIGNMENT);

#undef REWRITE_AND_EXPECT_REJECTION
    free(image);
}

int main(void)
{
    char template_path[] = "/tmp/mzg2-store-test-XXXXXX";
    char path[512];
    uint32_t layer;

    if (!mkdtemp(template_path)) {
        printf("could not create a temporary directory: %s\n",
               strerror(errno));
        return 1;
    }
    snprintf(root, sizeof(root), "%s", template_path);
    printf("store root: (temporary directory)\n");

    test_single_file();
    test_directory();
    test_rejections();

    for (layer = 1u; layer <= MZG2_FIXTURE_LAYERS; ++layer) {
        sidecar_path(path, sizeof(path), layer);
        (void)unlink(path);
    }
    (void)rmdir(root);
    return mzg2_test_report("test_store");
}
