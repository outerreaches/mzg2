/*
 * Host-side pre-decode validation: every structural invariant of
 * docs/format.md 6 and 8.1, checked before a decoder may touch a
 * destination. Fails closed with a message naming the first violation.
 */
#include "mzg2/mzg2.h"

#include <string.h>

#include "internal.h"

/*
 * Two rules here are stricter than the qualified Kimi K3 reader, and both
 * are satisfied by every block that reader ever produced:
 *
 *   - descriptor_offset must be a multiple of 8. The K3 reader expressed this
 *     as `% alignof(tile_descriptor)`, but the descriptor is a packed struct,
 *     so its alignment is 1 and the check was vacuous. The specification
 *     states 8; the encoder places the table at 8560.
 *   - a descriptor's checksum field must have its high 32 bits zero. The K3
 *     reader compared only the low word; the encoder writes a u32.
 *
 * Recorded in docs/extraction.md rather than silently relaxed, so the core
 * and the specification agree.
 */
bool mzg2_block_validate(const mzg2_profile *profile, uint32_t layer,
                         uint32_t expert, const void *block,
                         uint32_t block_bytes, mzg2_block_view *view,
                         char *error, size_t error_size)
{
    const mzg2_block_header *header;
    const mzg2_model_entry *models;
    const mzg2_tile_descriptor *descriptors;
    const uint8_t *bytes = (const uint8_t *)block;
    uint32_t expected_tiles;
    uint32_t tile;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (view) {
        memset(view, 0, sizeof(*view));
    }
    if (!profile || !block || !view) {
        mzg2_set_error(error, error_size, "invalid block validation arguments");
        return false;
    }
    if (!mzg2_profile_check(profile, error, error_size)) {
        return false;
    }
    if ((uintptr_t)block % _Alignof(mzg2_model_entry) != 0u) {
        mzg2_set_error(error, error_size, "block base must be 4-byte aligned");
        return false;
    }
    if (layer == 0u || layer > profile->max_layers ||
        expert >= profile->experts_per_layer) {
        mzg2_set_error(error, error_size, "layer %u expert %u out of profile",
                       layer, expert);
        return false;
    }
    if (block_bytes < sizeof(mzg2_block_header)) {
        mzg2_set_error(error, error_size, "block of %u bytes has no header",
                       block_bytes);
        return false;
    }
    expected_tiles = mzg2_profile_tile_count(profile);
    header = (const mzg2_block_header *)block;

    if (memcmp(header->magic, MZG2_BLOCK_MAGIC, 8u) != 0 ||
        header->version != MZG2_VERSION ||
        header->header_bytes != sizeof(*header) || header->flags != 0u ||
        header->layer != layer || header->expert != expert ||
        header->tile_bytes != profile->tile_bytes ||
        header->output_bytes != profile->output_bytes ||
        header->tile_count != expected_tiles ||
        header->block_bytes != block_bytes) {
        mzg2_set_error(error, error_size,
                       "block header mismatch layer %u expert %u", layer,
                       expert);
        return false;
    }
    /* Format v2 carries a fixed-size model entry regardless of how many
     * models a profile uses; only the used tables are validated below. */
    if (header->model_offset < header->header_bytes ||
        header->model_offset % 4u != 0u ||
        header->model_bytes != sizeof(mzg2_model_entry) ||
        header->model_offset > block_bytes ||
        header->model_bytes > block_bytes - header->model_offset) {
        mzg2_set_error(error, error_size,
                       "block model region invalid layer %u expert %u", layer,
                       expert);
        return false;
    }
    if (header->descriptor_offset <
            header->model_offset + header->model_bytes ||
        header->descriptor_offset % 8u != 0u ||
        header->descriptor_bytes !=
            expected_tiles * (uint32_t)sizeof(mzg2_tile_descriptor) ||
        header->descriptor_offset > block_bytes ||
        header->descriptor_bytes > block_bytes - header->descriptor_offset) {
        mzg2_set_error(error, error_size,
                       "block descriptor region invalid layer %u expert %u",
                       layer, expert);
        return false;
    }
    if (header->payload_offset <
            header->descriptor_offset + header->descriptor_bytes ||
        header->payload_offset % 4u != 0u ||
        header->payload_offset > block_bytes) {
        mzg2_set_error(error, error_size,
                       "block payload region invalid layer %u expert %u",
                       layer, expert);
        return false;
    }

    models = (const mzg2_model_entry *)(bytes + header->model_offset);
    if (!mzg2_model_entry_validate(models, profile)) {
        mzg2_set_error(error, error_size,
                       "block model table invalid layer %u expert %u", layer,
                       expert);
        return false;
    }

    descriptors =
        (const mzg2_tile_descriptor *)(bytes + header->descriptor_offset);
    for (tile = 0u; tile < expected_tiles; ++tile) {
        const mzg2_tile_descriptor *descriptor = &descriptors[tile];
        const uint32_t model = descriptor->kind & MZG2_MODEL_MASK;
        const uint32_t output_bytes =
            mzg2_profile_tile_output_bytes(profile, tile);

        if (model >= profile->model_count ||
            (descriptor->kind & ~(MZG2_TILE_RAW | MZG2_MODEL_MASK)) != 0u ||
            descriptor->reserved != 0u ||
            (descriptor->checksum >> 32u) != 0u) {
            mzg2_set_error(error, error_size,
                           "tile %u kind/reserved invalid layer %u expert %u",
                           tile, layer, expert);
            return false;
        }
        if (descriptor->output_bytes != output_bytes ||
            descriptor->output_offset != tile * profile->tile_bytes) {
            mzg2_set_error(error, error_size,
                           "tile %u output extent invalid layer %u expert %u",
                           tile, layer, expert);
            return false;
        }
        if (descriptor->payload_offset % 4u != 0u ||
            descriptor->payload_offset > block_bytes ||
            descriptor->payload_bytes >
                block_bytes - descriptor->payload_offset) {
            mzg2_set_error(error, error_size,
                           "tile %u payload extent invalid layer %u expert %u",
                           tile, layer, expert);
            return false;
        }
        if (descriptor->kind & MZG2_TILE_RAW) {
            if (descriptor->payload_bytes != descriptor->output_bytes) {
                mzg2_set_error(error, error_size,
                               "tile %u raw payload invalid layer %u expert %u",
                               tile, layer, expert);
                return false;
            }
        } else {
            const uint32_t states = MZG2_LANES * (uint32_t)sizeof(uint32_t);

            if (descriptor->payload_bytes < states ||
                (descriptor->payload_bytes - states) % 2u != 0u) {
                mzg2_set_error(
                    error, error_size,
                    "tile %u coded payload invalid layer %u expert %u", tile,
                    layer, expert);
                return false;
            }
        }
    }

    view->header = header;
    view->models = models;
    view->descriptors = descriptors;
    view->tile_count = expected_tiles;
    return true;
}
