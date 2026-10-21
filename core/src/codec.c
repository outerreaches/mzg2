/*
 * CPU reference codec.
 *
 * This is the normative implementation of docs/format.md 8: the encoder
 * produces the byte layout the specification describes, and the decoder
 * reproduces the wave32 lane interleave on a single thread -- a round's
 * renormalization words are consumed in increasing lane order, which is
 * exactly what a 32-lane wave does with its ballot mask. One thread
 * therefore decodes what a wave decodes, bit for bit.
 *
 * Portable C: no HIP, no model knowledge, no entropy-coding shortcut. It is
 * the oracle for the GPU decoder and the tool that builds stores.
 */
#include "mzg2/mzg2.h"

#include <stdlib.h>
#include <string.h>

#include "internal.h"

typedef struct {
    mzg2_tile_descriptor descriptor;
    uint8_t *payload;
    uint32_t payload_bytes;
} encoded_tile;

static uint32_t tile_checksum(const uint8_t *data, uint32_t bytes)
{
    uint32_t checksum = 0u;
    uint32_t offset;

    for (offset = 0u; offset < bytes; offset += 4u) {
        uint32_t word;

        memcpy(&word, data + offset, sizeof(word));
        checksum ^= mzg2_word_checksum(word, offset / 4u);
    }
    return checksum;
}

/*
 * The symbol lane `lane` decodes in round `round`, read from a decoded tile.
 * The layout is lane-interleaved 128-byte groups: a lane owns one dword per
 * group, and fills it with 8 four-bit or 4 eight-bit symbols.
 */
static uint8_t tile_value(const uint8_t *data, uint8_t symbol_bits,
                          uint32_t round, uint32_t lane)
{
    if (symbol_bits == 4u) {
        const uint32_t dword = round / 8u;
        const uint32_t nibble = round % 8u;
        const uint32_t index =
            (dword * MZG2_LANES + lane) * 4u + nibble / 2u;

        return (uint8_t)((data[index] >> ((nibble & 1u) * 4u)) & 0x0fu);
    }
    {
        const uint32_t dword = round / 4u;
        const uint32_t byte = round % 4u;

        return data[(dword * MZG2_LANES + lane) * 4u + byte];
    }
}

static void tile_store_value(uint8_t *destination, uint8_t symbol_bits,
                             uint32_t round, uint32_t lane, uint8_t value)
{
    if (symbol_bits == 4u) {
        const uint32_t dword = round / 8u;
        const uint32_t nibble = round % 8u;
        const uint32_t index =
            (dword * MZG2_LANES + lane) * 4u + nibble / 2u;

        destination[index] |= (uint8_t)(value << ((nibble & 1u) * 4u));
        return;
    }
    {
        const uint32_t dword = round / 4u;
        const uint32_t byte = round % 4u;

        destination[(dword * MZG2_LANES + lane) * 4u + byte] = value;
    }
}

void mzg2_encoded_release(mzg2_encoded *encoded)
{
    if (!encoded) {
        return;
    }
    free(encoded->bytes);
    encoded->bytes = NULL;
    encoded->size = 0u;
    encoded->tile_count = 0u;
    encoded->raw_tiles = 0u;
}

static void release_tiles(encoded_tile *tiles, uint32_t count)
{
    uint32_t index;

    if (!tiles) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        free(tiles[index].payload);
    }
    free(tiles);
}

/*
 * Encode one tile. Returns false only on an internal inconsistency (a value
 * outside the model's alphabet, or an arithmetic range violation); falling
 * back to the raw escape is a success, not a failure.
 */
static bool encode_tile(const uint8_t *data, uint32_t output_bytes,
                        uint32_t model_index, uint8_t symbol_bits,
                        const mzg2_built_model *model, encoded_tile *tile)
{
    uint32_t states[MZG2_LANES];
    uint16_t *words;
    uint32_t word_count = 0u;
    uint32_t word_capacity;
    const uint32_t symbols = output_bytes * mzg2_symbols_per_byte(symbol_bits);
    const uint32_t rounds = symbols / MZG2_LANES;
    uint32_t reverse_round;
    uint32_t lane;

    if (output_bytes == 0u ||
        output_bytes % MZG2_TILE_OUTPUT_GRANULE != 0u) {
        return false;
    }
    /* A symbol pushes at most two renormalization words. */
    word_capacity = symbols * 2u + MZG2_LANES;
    words = (uint16_t *)malloc((size_t)word_capacity * sizeof(*words));
    if (!words) {
        return false;
    }
    for (lane = 0u; lane < MZG2_LANES; ++lane) {
        states[lane] = MZG2_LOWER_BOUND;
    }

    /* rANS encodes in reverse, so the decoder meets the symbols in order. */
    for (reverse_round = rounds; reverse_round > 0u; --reverse_round) {
        const uint32_t round = reverse_round - 1u;
        uint32_t reverse_lane;

        for (reverse_lane = MZG2_LANES; reverse_lane > 0u; --reverse_lane) {
            const uint32_t active = reverse_lane - 1u;
            const uint8_t value =
                tile_value(data, symbol_bits, round, active);
            const uint8_t symbol = model->symbol_for_value[value];
            uint32_t frequency;
            uint32_t cumulative;
            uint32_t state;
            uint64_t maximum;
            uint64_t encoded;

            if (symbol == 0xffu || symbol >= model->table.symbol_count) {
                free(words);
                return false;
            }
            frequency = model->table.frequency[symbol];
            cumulative = model->table.cumulative[symbol];
            state = states[active];
            maximum = (uint64_t)frequency << 20u;
            while (state >= maximum) {
                if (word_count >= word_capacity) {
                    free(words);
                    return false;
                }
                words[word_count++] = (uint16_t)state;
                state >>= 16u;
            }
            encoded = ((uint64_t)(state / frequency) << MZG2_SCALE_BITS) +
                      state % frequency + cumulative;
            if (encoded > UINT32_MAX) {
                free(words);
                return false;
            }
            states[active] = (uint32_t)encoded;
        }
    }
    /* The word stream is written in decode order. */
    for (lane = 0u; lane < word_count / 2u; ++lane) {
        const uint16_t swap = words[lane];

        words[lane] = words[word_count - 1u - lane];
        words[word_count - 1u - lane] = swap;
    }

    memset(&tile->descriptor, 0, sizeof(tile->descriptor));
    tile->descriptor.output_bytes = output_bytes;
    tile->descriptor.kind = model_index;
    tile->descriptor.checksum = tile_checksum(data, output_bytes);

    {
        const uint32_t state_bytes = MZG2_LANES * (uint32_t)sizeof(uint32_t);
        const uint32_t coded_bytes =
            state_bytes + word_count * (uint32_t)sizeof(uint16_t);

        if (coded_bytes >= output_bytes) {
            /* Incompressible: an escape costs one tile, not a larger block. */
            tile->descriptor.kind |= MZG2_TILE_RAW;
            tile->payload = (uint8_t *)malloc(output_bytes);
            if (!tile->payload) {
                free(words);
                return false;
            }
            memcpy(tile->payload, data, output_bytes);
            tile->payload_bytes = output_bytes;
        } else {
            tile->payload = (uint8_t *)malloc(coded_bytes);
            if (!tile->payload) {
                free(words);
                return false;
            }
            memcpy(tile->payload, states, state_bytes);
            memcpy(tile->payload + state_bytes, words,
                   (size_t)word_count * sizeof(*words));
            tile->payload_bytes = coded_bytes;
        }
    }
    free(words);
    return true;
}

bool mzg2_models_from_planes(const mzg2_profile *profile, const void *source,
                             const mzg2_plane *planes, uint32_t plane_count,
                             mzg2_built_model *models, mzg2_model_entry *entry,
                             char *error, size_t error_size)
{
    uint64_t (*counts)[256];
    const uint8_t *bytes = (const uint8_t *)source;
    uint32_t plane;
    uint32_t model;
    bool ok = true;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (!profile || !source || !planes || !models || !entry ||
        plane_count == 0u) {
        mzg2_set_error(error, error_size, "invalid model build arguments");
        return false;
    }
    if (!mzg2_profile_check(profile, error, error_size)) {
        return false;
    }
    counts = (uint64_t (*)[256])calloc(profile->model_count, sizeof(*counts));
    if (!counts) {
        mzg2_set_error(error, error_size, "symbol count allocation failed");
        return false;
    }
    for (plane = 0u; plane < plane_count; ++plane) {
        const mzg2_plane *item = &planes[plane];
        uint32_t offset;

        if (item->model >= profile->model_count) {
            mzg2_set_error(error, error_size, "plane %u names model %u", plane,
                           item->model);
            free(counts);
            return false;
        }
        if (item->offset > profile->output_bytes ||
            item->bytes > profile->output_bytes - item->offset) {
            mzg2_set_error(error, error_size, "plane %u extent out of payload",
                           plane);
            free(counts);
            return false;
        }
        if (profile->model[item->model].symbol_bits == 4u) {
            for (offset = 0u; offset < item->bytes; ++offset) {
                const uint8_t byte = bytes[item->offset + offset];

                ++counts[item->model][byte & 0x0fu];
                ++counts[item->model][byte >> 4u];
            }
        } else {
            for (offset = 0u; offset < item->bytes; ++offset) {
                ++counts[item->model][bytes[item->offset + offset]];
            }
        }
    }
    memset(entry, 0, sizeof(*entry));
    for (model = 0u; model < profile->model_count; ++model) {
        if (!mzg2_model_build(counts[model], &profile->model[model],
                              &models[model])) {
            mzg2_set_error(error, error_size,
                           "model %u build failed (alphabet too large, or no "
                           "permitted symbol observed)",
                           model);
            ok = false;
            break;
        }
        entry->model[model] = models[model].table;
    }
    free(counts);
    return ok;
}

bool mzg2_encode_block(const mzg2_profile *profile, uint32_t layer,
                       uint32_t expert, const void *source,
                       const mzg2_plane *planes, uint32_t plane_count,
                       const mzg2_built_model *models,
                       const mzg2_model_entry *entry, mzg2_encoded *out,
                       char *error, size_t error_size)
{
    const uint8_t *bytes = (const uint8_t *)source;
    const uint32_t expected_tiles = mzg2_profile_tile_count(profile);
    encoded_tile *tiles;
    mzg2_block_header *header;
    mzg2_tile_descriptor *descriptors;
    uint32_t model_offset;
    uint32_t descriptor_offset;
    uint32_t descriptor_bytes;
    uint32_t payload_offset;
    uint64_t cursor;
    uint32_t produced = 0u;
    uint32_t plane;
    uint32_t index;
    uint32_t expected_offset = 0u;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (!profile || !source || !planes || !models || !entry || !out ||
        plane_count == 0u) {
        mzg2_set_error(error, error_size, "invalid encode arguments");
        return false;
    }
    if (!mzg2_profile_check(profile, error, error_size)) {
        return false;
    }
    if (layer == 0u || layer > profile->max_layers ||
        expert >= profile->experts_per_layer) {
        mzg2_set_error(error, error_size, "layer %u expert %u out of profile",
                       layer, expert);
        return false;
    }
    /*
     * Planes must tile the payload in output order, because a descriptor's
     * output_offset is required to be its tile index times the tile size.
     * Every plane boundary therefore lands on a tile boundary; only the last
     * tile of the payload may be short.
     */
    for (plane = 0u; plane < plane_count; ++plane) {
        if (planes[plane].offset != expected_offset ||
            planes[plane].bytes == 0u ||
            planes[plane].bytes > profile->output_bytes - expected_offset ||
            planes[plane].model >= profile->model_count) {
            mzg2_set_error(error, error_size,
                           "plane %u is not the next payload run", plane);
            return false;
        }
        if (plane + 1u < plane_count &&
            planes[plane].bytes % profile->tile_bytes != 0u) {
            mzg2_set_error(error, error_size,
                           "plane %u of %u bytes does not end on a tile "
                           "boundary",
                           plane, planes[plane].bytes);
            return false;
        }
        expected_offset += planes[plane].bytes;
    }
    if (expected_offset != profile->output_bytes) {
        mzg2_set_error(error, error_size,
                       "planes cover %u of %u payload bytes", expected_offset,
                       profile->output_bytes);
        return false;
    }

    tiles = (encoded_tile *)calloc(expected_tiles, sizeof(*tiles));
    if (!tiles) {
        mzg2_set_error(error, error_size, "tile allocation failed");
        return false;
    }
    for (plane = 0u; plane < plane_count; ++plane) {
        const mzg2_plane *item = &planes[plane];
        const uint8_t symbol_bits = profile->model[item->model].symbol_bits;
        uint32_t position;

        for (position = 0u; position < item->bytes;
             position += profile->tile_bytes) {
            const uint32_t remaining = item->bytes - position;
            const uint32_t output_bytes = remaining < profile->tile_bytes
                                              ? remaining
                                              : profile->tile_bytes;

            if (produced >= expected_tiles) {
                mzg2_set_error(error, error_size, "tile count overflow");
                release_tiles(tiles, produced);
                return false;
            }
            if (!encode_tile(bytes + item->offset + position, output_bytes,
                             item->model, symbol_bits, &models[item->model],
                             &tiles[produced])) {
                mzg2_set_error(error, error_size,
                               "encode failed at tile %u (plane %u)", produced,
                               plane);
                release_tiles(tiles, produced);
                return false;
            }
            tiles[produced].descriptor.output_offset = item->offset + position;
            if (tiles[produced].descriptor.kind & MZG2_TILE_RAW) {
                ++out->raw_tiles;
            }
            ++produced;
        }
    }
    if (produced != expected_tiles) {
        mzg2_set_error(error, error_size, "produced %u of %u tiles", produced,
                       expected_tiles);
        release_tiles(tiles, produced);
        return false;
    }

    model_offset = (uint32_t)sizeof(mzg2_block_header);
    descriptor_offset = model_offset + (uint32_t)sizeof(mzg2_model_entry);
    descriptor_bytes =
        expected_tiles * (uint32_t)sizeof(mzg2_tile_descriptor);
    payload_offset = (uint32_t)mzg2_align_up(
        (uint64_t)descriptor_offset + descriptor_bytes, MZG2_ALIGNMENT);
    cursor = payload_offset;
    for (index = 0u; index < expected_tiles; ++index) {
        cursor = mzg2_align_up(cursor, 4u);
        if (cursor > UINT32_MAX ||
            tiles[index].payload_bytes > UINT32_MAX - cursor) {
            mzg2_set_error(error, error_size, "block layout exceeds 32 bits");
            release_tiles(tiles, expected_tiles);
            return false;
        }
        tiles[index].descriptor.payload_offset = (uint32_t)cursor;
        tiles[index].descriptor.payload_bytes = tiles[index].payload_bytes;
        cursor += tiles[index].payload_bytes;
    }
    cursor = mzg2_align_up(cursor, MZG2_ALIGNMENT);
    if (cursor > UINT32_MAX) {
        mzg2_set_error(error, error_size, "block layout exceeds 32 bits");
        release_tiles(tiles, expected_tiles);
        return false;
    }

    out->bytes = (uint8_t *)calloc(1u, (size_t)cursor);
    if (!out->bytes) {
        mzg2_set_error(error, error_size, "block allocation failed");
        release_tiles(tiles, expected_tiles);
        return false;
    }
    out->size = (uint32_t)cursor;
    out->tile_count = expected_tiles;

    header = (mzg2_block_header *)out->bytes;
    memcpy(header->magic, MZG2_BLOCK_MAGIC, 8u);
    header->version = MZG2_VERSION;
    header->header_bytes = (uint32_t)sizeof(*header);
    header->layer = layer;
    header->expert = expert;
    header->tile_bytes = profile->tile_bytes;
    header->tile_count = expected_tiles;
    header->output_bytes = profile->output_bytes;
    header->descriptor_offset = descriptor_offset;
    header->descriptor_bytes = descriptor_bytes;
    header->payload_offset = payload_offset;
    header->block_bytes = out->size;
    header->flags = 0u;
    header->model_offset = model_offset;
    header->model_bytes = (uint32_t)sizeof(mzg2_model_entry);
    memcpy(out->bytes + model_offset, entry, sizeof(*entry));

    descriptors = (mzg2_tile_descriptor *)(out->bytes + descriptor_offset);
    for (index = 0u; index < expected_tiles; ++index) {
        descriptors[index] = tiles[index].descriptor;
        memcpy(out->bytes + tiles[index].descriptor.payload_offset,
               tiles[index].payload, tiles[index].payload_bytes);
    }
    release_tiles(tiles, expected_tiles);
    return true;
}

uint32_t mzg2_decode_block(const mzg2_profile *profile, uint32_t layer,
                           uint32_t expert, const void *block,
                           uint32_t block_bytes, void *destination,
                           uint32_t destination_bytes, char *error,
                           size_t error_size)
{
    mzg2_block_view view;
    const uint8_t *bytes = (const uint8_t *)block;
    uint8_t *output = (uint8_t *)destination;
    uint32_t tile;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (!profile || !block || !destination) {
        mzg2_set_error(error, error_size, "invalid decode arguments");
        return MZG2_ERROR_BOUNDS;
    }
    if (destination_bytes < profile->output_bytes) {
        mzg2_set_error(error, error_size,
                       "destination of %u bytes is short of %u",
                       destination_bytes, profile->output_bytes);
        return MZG2_ERROR_BOUNDS;
    }
    /* Decode is out-of-place. Subtractions avoid overflowing pointer-range
     * endpoints, and reject partial overlap in either direction. */
    {
        const uintptr_t source_address = (uintptr_t)block;
        const uintptr_t output_address = (uintptr_t)destination;
        const bool overlaps = source_address <= output_address
            ? output_address - source_address < block_bytes
            : source_address - output_address < profile->output_bytes;

        if (overlaps) {
            mzg2_set_error(error, error_size, "block and destination overlap");
            return MZG2_ERROR_BOUNDS;
        }
    }
    if (!mzg2_block_validate(profile, layer, expert, block, block_bytes, &view,
                             error, error_size)) {
        return MZG2_ERROR_BOUNDS;
    }

    for (tile = 0u; tile < view.tile_count; ++tile) {
        const mzg2_tile_descriptor *descriptor = &view.descriptors[tile];
        const uint32_t model_index = descriptor->kind & MZG2_MODEL_MASK;
        const mzg2_model *model = &view.models->model[model_index];
        const uint8_t symbol_bits = profile->model[model_index].symbol_bits;
        const uint8_t *payload = bytes + descriptor->payload_offset;
        uint8_t *target = output + descriptor->output_offset;

        memset(target, 0, descriptor->output_bytes);
        if (descriptor->kind & MZG2_TILE_RAW) {
            memcpy(target, payload, descriptor->output_bytes);
        } else {
            const uint32_t state_bytes =
                MZG2_LANES * (uint32_t)sizeof(uint32_t);
            const uint32_t word_count =
                (descriptor->payload_bytes - state_bytes) / 2u;
            const uint32_t rounds = descriptor->output_bytes *
                                    mzg2_symbols_per_byte(symbol_bits) /
                                    MZG2_LANES;
            uint32_t states[MZG2_LANES];
            uint32_t cursor = 0u;
            uint32_t lane;
            uint32_t round;

            memcpy(states, payload, state_bytes);
            for (lane = 0u; lane < MZG2_LANES; ++lane) {
                if (states[lane] < MZG2_LOWER_BOUND) {
                    mzg2_set_error(error, error_size,
                                   "tile %u lane %u initial state below the "
                                   "lower bound",
                                   tile, lane);
                    return MZG2_ERROR_STATE;
                }
            }
            for (round = 0u; round < rounds; ++round) {
                /* Lane order within a round is the format's word order. */
                for (lane = 0u; lane < MZG2_LANES; ++lane) {
                    uint32_t state = states[lane];
                    const uint32_t slot = state & (MZG2_MODEL_SLOTS - 1u);
                    const uint32_t symbol = model->decode[slot];

                    if (symbol >= model->symbol_count) {
                        mzg2_set_error(error, error_size,
                                       "tile %u round %u lane %u decoded "
                                       "symbol %u out of range",
                                       tile, round, lane, symbol);
                        return MZG2_ERROR_STATE;
                    }
                    state = model->frequency[symbol] *
                                (state >> MZG2_SCALE_BITS) +
                            slot - model->cumulative[symbol];
                    if (state < MZG2_LOWER_BOUND) {
                        const uint16_t *words =
                            (const uint16_t *)(payload + state_bytes);

                        if (cursor >= word_count) {
                            mzg2_set_error(error, error_size,
                                           "tile %u round %u lane %u ran past "
                                           "the word stream",
                                           tile, round, lane);
                            return MZG2_ERROR_STATE;
                        }
                        state = (state << 16u) | words[cursor++];
                    }
                    states[lane] = state;
                    tile_store_value(target, symbol_bits, round, lane,
                                     model->values[symbol]);
                }
            }
            if (cursor != word_count) {
                mzg2_set_error(error, error_size,
                               "tile %u left %u of %u words unread", tile,
                               word_count - cursor, word_count);
                return MZG2_ERROR_TERMINAL;
            }
            for (lane = 0u; lane < MZG2_LANES; ++lane) {
                if (states[lane] != MZG2_LOWER_BOUND) {
                    mzg2_set_error(error, error_size,
                                   "tile %u lane %u final state %u is not the "
                                   "lower bound",
                                   tile, lane, states[lane]);
                    return MZG2_ERROR_TERMINAL;
                }
            }
        }
        if (tile_checksum(target, descriptor->output_bytes) !=
            (uint32_t)descriptor->checksum) {
            mzg2_set_error(error, error_size, "tile %u checksum mismatch",
                           tile);
            return MZG2_ERROR_CHECKSUM;
        }
    }
    return 0u;
}
