/*
 * Store reader: sidecar headers, expert index, and the (layer, expert) ->
 * extent map. Fail-closed at open, so a serving engine never discovers a
 * malformed store mid-request.
 *
 * Each sidecar is opened twice: a buffered descriptor for metadata and tool
 * reads, and an O_DIRECT descriptor handed out in a span so an engine can
 * read a block straight into GPU-mapped staging.
 */
#define _GNU_SOURCE

#include "mzg2/mzg2.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"

typedef struct {
    int fd;
    int direct_fd;
    uint64_t bytes;
} store_file;

typedef struct {
    uint32_t file;
    bool present;
    uint64_t offset;
    uint32_t bytes;
} map_entry;

struct mzg2_store {
    const mzg2_profile *profile;
    store_file *files;
    uint32_t file_count;
    uint32_t file_capacity;
    uint32_t max_block_bytes;
    uint64_t present_count;
    map_entry *map;
};

static bool pread_full(int fd, void *buffer, size_t bytes, uint64_t offset)
{
    uint8_t *cursor = (uint8_t *)buffer;

    while (bytes != 0u) {
        const ssize_t result = pread(fd, cursor, bytes, (off_t)offset);

        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        cursor += (size_t)result;
        bytes -= (size_t)result;
        offset += (uint64_t)result;
    }
    return true;
}

static bool header_valid(const mzg2_store *store,
                         const mzg2_file_header *header, uint64_t file_bytes)
{
    const mzg2_profile *profile = store->profile;

    return memcmp(header->magic, profile->magic, sizeof(header->magic)) == 0 &&
           header->version == MZG2_VERSION &&
           header->header_bytes == MZG2_FILE_HEADER_BYTES &&
           header->alignment == MZG2_ALIGNMENT && header->layer != 0u &&
           header->layer <= profile->max_layers && header->flags == 0u &&
           header->expert_count != 0u &&
           header->expert_count <= profile->experts_per_layer &&
           header->index_entry_bytes == sizeof(mzg2_index_entry) &&
           header->model_entry_bytes == 0u &&
           header->tile_bytes == profile->tile_bytes &&
           header->index_offset == MZG2_FILE_HEADER_BYTES &&
           header->index_bytes ==
               (uint64_t)header->expert_count * sizeof(mzg2_index_entry) &&
           header->model_offset == 0u && header->model_bytes == 0u &&
           header->data_offset >= header->index_offset + header->index_bytes &&
           header->data_offset % MZG2_ALIGNMENT == 0u &&
           header->max_block_bytes != 0u &&
           header->max_block_bytes <= UINT32_MAX &&
           header->max_block_bytes % MZG2_ALIGNMENT == 0u &&
           header->data_offset <= file_bytes;
}

static bool load_file(mzg2_store *store, const char *path, char *error,
                      size_t error_size)
{
    const mzg2_profile *profile = store->profile;
    mzg2_file_header header;
    mzg2_index_entry *index = NULL;
    struct stat status;
    int fd;
    int direct_fd;
    uint32_t file_index;
    uint32_t item;

    memset(&header, 0, sizeof(header));
    memset(&status, 0, sizeof(status));
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        mzg2_set_error(error, error_size, "open sidecar: %s", strerror(errno));
        return false;
    }
    direct_fd = open(path, O_RDONLY | O_CLOEXEC | O_DIRECT);
    if (direct_fd < 0) {
        mzg2_set_error(error, error_size, "open sidecar with direct I/O: %s",
                       strerror(errno));
        close(fd);
        return false;
    }
    if (fstat(fd, &status) != 0 ||
        !pread_full(fd, &header, sizeof(header), 0u)) {
        mzg2_set_error(error, error_size, "read sidecar header: %s",
                       strerror(errno));
        goto fail;
    }
    if (!header_valid(store, &header, (uint64_t)status.st_size)) {
        mzg2_set_error(error, error_size,
                       "sidecar header fails the profile's rules");
        goto fail;
    }
    if (profile->source_manifest_sha256 &&
        memcmp(header.source_manifest_sha256, profile->source_manifest_sha256,
               sizeof(header.source_manifest_sha256)) != 0) {
        mzg2_set_error(error, error_size,
                       "sidecar is not bound to the profile's source manifest");
        goto fail;
    }
    if (store->file_count >= store->file_capacity) {
        mzg2_set_error(error, error_size, "more sidecars than the profile's %u",
                       store->file_capacity);
        goto fail;
    }
    index = (mzg2_index_entry *)malloc((size_t)header.index_bytes);
    if (!index ||
        !pread_full(fd, index, (size_t)header.index_bytes,
                    header.index_offset)) {
        mzg2_set_error(error, error_size, "read sidecar index");
        goto fail;
    }

    file_index = store->file_count;
    for (item = 0u; item < header.expert_count; ++item) {
        const mzg2_index_entry *entry = &index[item];
        map_entry *mapping;

        if (entry->layer != header.layer ||
            entry->expert >= profile->experts_per_layer ||
            entry->block_bytes == 0u ||
            entry->block_bytes % MZG2_ALIGNMENT != 0u ||
            entry->block_bytes > header.max_block_bytes ||
            entry->block_offset % MZG2_ALIGNMENT != 0u ||
            entry->block_offset < header.data_offset ||
            entry->block_offset > (uint64_t)status.st_size ||
            entry->block_bytes >
                (uint64_t)status.st_size - entry->block_offset) {
            mzg2_set_error(error, error_size, "index entry %u is invalid",
                           item);
            goto fail;
        }
        mapping = &store->map[(uint64_t)(entry->layer - 1u) *
                                  profile->experts_per_layer +
                              entry->expert];
        if (mapping->present) {
            mzg2_set_error(error, error_size,
                           "duplicate layer %u expert %u", entry->layer,
                           entry->expert);
            goto fail;
        }
        mapping->file = file_index;
        mapping->present = true;
        mapping->offset = entry->block_offset;
        mapping->bytes = entry->block_bytes;
        ++store->present_count;
    }
    free(index);
    store->files[file_index].fd = fd;
    store->files[file_index].direct_fd = direct_fd;
    store->files[file_index].bytes = (uint64_t)status.st_size;
    ++store->file_count;
    if (header.max_block_bytes > store->max_block_bytes) {
        store->max_block_bytes = (uint32_t)header.max_block_bytes;
    }
    return true;

fail:
    free(index);
    close(direct_fd);
    close(fd);
    return false;
}

bool mzg2_store_open(const mzg2_profile *profile, const char *path,
                     mzg2_store **out, char *error, size_t error_size)
{
    mzg2_store *store;
    struct stat root;
    uint32_t file;

    if (out) {
        *out = NULL;
    }
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!out) {
        mzg2_set_error(error, error_size, "invalid store output");
        return false;
    }
    if (!mzg2_profile_check(profile, error, error_size)) {
        return false;
    }
    if (!path || path[0] != '/') {
        mzg2_set_error(error, error_size, "store path must be absolute");
        return false;
    }
    memset(&root, 0, sizeof(root));
    if (stat(path, &root) != 0 ||
        (!S_ISREG(root.st_mode) && !S_ISDIR(root.st_mode))) {
        mzg2_set_error(error, error_size, "store path is not a file or "
                                          "directory");
        return false;
    }

    store = (mzg2_store *)calloc(1u, sizeof(*store));
    if (!store) {
        mzg2_set_error(error, error_size, "store allocation failed");
        return false;
    }
    store->profile = profile;
    store->file_capacity = profile->max_layers;
    store->files = (store_file *)calloc(store->file_capacity,
                                        sizeof(*store->files));
    store->map = (map_entry *)calloc((size_t)profile->max_layers *
                                         profile->experts_per_layer,
                                     sizeof(*store->map));
    if (!store->files || !store->map) {
        mzg2_store_destroy(store);
        mzg2_set_error(error, error_size, "store allocation failed");
        return false;
    }
    for (file = 0u; file < store->file_capacity; ++file) {
        store->files[file].fd = -1;
        store->files[file].direct_fd = -1;
    }

    if (S_ISREG(root.st_mode)) {
        if (!load_file(store, path, error, error_size)) {
            mzg2_store_destroy(store);
            return false;
        }
    } else {
        uint32_t layer;

        for (layer = 1u; layer <= profile->max_layers; ++layer) {
            char sidecar[4096];
            char name[256];
            int length;

            length = snprintf(name, sizeof(name), profile->sidecar_pattern,
                              layer);
            if (length < 0 || (size_t)length >= sizeof(name)) {
                mzg2_store_destroy(store);
                mzg2_set_error(error, error_size,
                               "sidecar name for layer %u does not fit", layer);
                return false;
            }
            length = snprintf(sidecar, sizeof(sidecar), "%s/%s", path, name);
            if (length < 0 || (size_t)length >= sizeof(sidecar)) {
                mzg2_store_destroy(store);
                mzg2_set_error(error, error_size,
                               "sidecar path for layer %u does not fit", layer);
                return false;
            }
            if (!load_file(store, sidecar, error, error_size)) {
                mzg2_store_destroy(store);
                return false;
            }
        }
        if (profile->require_complete_directory) {
            const uint64_t expected = (uint64_t)profile->max_layers *
                                      profile->experts_per_layer;

            if (store->present_count != expected) {
                mzg2_set_error(error, error_size,
                               "store is incomplete: %llu of %llu experts",
                               (unsigned long long)store->present_count,
                               (unsigned long long)expected);
                mzg2_store_destroy(store);
                return false;
            }
        }
    }
    *out = store;
    return true;
}

void mzg2_store_destroy(mzg2_store *store)
{
    uint32_t file;

    if (!store) {
        return;
    }
    for (file = 0u; store->files && file < store->file_count; ++file) {
        if (store->files[file].direct_fd >= 0) {
            (void)close(store->files[file].direct_fd);
        }
        if (store->files[file].fd >= 0) {
            (void)close(store->files[file].fd);
        }
    }
    free(store->files);
    free(store->map);
    free(store);
}

uint32_t mzg2_store_max_block_bytes(const mzg2_store *store)
{
    return store ? store->max_block_bytes : 0u;
}

uint64_t mzg2_store_present_count(const mzg2_store *store)
{
    return store ? store->present_count : 0u;
}

static const map_entry *lookup(const mzg2_store *store, uint32_t layer,
                               uint32_t expert)
{
    const map_entry *mapping;

    if (!store || layer == 0u || layer > store->profile->max_layers ||
        expert >= store->profile->experts_per_layer) {
        return NULL;
    }
    mapping = &store->map[(uint64_t)(layer - 1u) *
                              store->profile->experts_per_layer +
                          expert];
    if (!mapping->present || mapping->file >= store->file_count) {
        return NULL;
    }
    return mapping;
}

bool mzg2_store_span(const mzg2_store *store, uint32_t layer, uint32_t expert,
                     mzg2_span *span)
{
    const map_entry *mapping;

    if (span) {
        memset(span, 0, sizeof(*span));
    }
    if (!span) {
        return false;
    }
    mapping = lookup(store, layer, expert);
    if (!mapping) {
        return false;
    }
    span->direct_fd = store->files[mapping->file].direct_fd;
    span->offset = mapping->offset;
    span->bytes = mapping->bytes;
    return true;
}

bool mzg2_store_read_block(const mzg2_store *store, uint32_t layer,
                           uint32_t expert, void *buffer,
                           uint32_t buffer_bytes, uint32_t *block_bytes,
                           char *error, size_t error_size)
{
    const map_entry *mapping;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (block_bytes) {
        *block_bytes = 0u;
    }
    if (!buffer) {
        mzg2_set_error(error, error_size, "no read destination");
        return false;
    }
    mapping = lookup(store, layer, expert);
    if (!mapping) {
        mzg2_set_error(error, error_size, "layer %u expert %u is not present",
                       layer, expert);
        return false;
    }
    if (buffer_bytes < mapping->bytes) {
        mzg2_set_error(error, error_size,
                       "buffer of %u bytes is short of the %u byte block",
                       buffer_bytes, mapping->bytes);
        return false;
    }
    if (!pread_full(store->files[mapping->file].fd, buffer, mapping->bytes,
                    mapping->offset)) {
        mzg2_set_error(error, error_size, "read layer %u expert %u: %s", layer,
                       expert, strerror(errno));
        return false;
    }
    if (block_bytes) {
        *block_bytes = mapping->bytes;
    }
    return true;
}
