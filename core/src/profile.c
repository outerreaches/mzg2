/* Profile consistency and derived geometry. */
#include "mzg2/mzg2.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"

void mzg2_set_error(char *error, size_t error_size, const char *format, ...)
{
    va_list arguments;

    if (!error || error_size == 0u) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

bool mzg2_value_allowed(const mzg2_model_spec *spec, uint32_t value)
{
    if (value >= (1u << spec->symbol_bits)) {
        return false;
    }
    if (!spec->allowed) {
        return true;
    }
    return (spec->allowed[value >> 3u] >> (value & 7u)) & 1u;
}

uint32_t mzg2_profile_tile_count(const mzg2_profile *profile)
{
    if (!profile || profile->tile_bytes == 0u) {
        return 0u;
    }
    return (profile->output_bytes + profile->tile_bytes - 1u) /
           profile->tile_bytes;
}

uint32_t mzg2_profile_tile_output_bytes(const mzg2_profile *profile,
                                        uint32_t tile_index)
{
    uint64_t start;

    if (!profile || profile->tile_bytes == 0u) {
        return 0u;
    }
    start = (uint64_t)tile_index * profile->tile_bytes;
    if (start >= profile->output_bytes) {
        return 0u;
    }
    if (profile->output_bytes - start < profile->tile_bytes) {
        return (uint32_t)(profile->output_bytes - start);
    }
    return profile->tile_bytes;
}

void mzg2_profile_tile_rules(const mzg2_profile *profile,
                             mzg2_tile_rules *rules)
{
    uint32_t model;

    if (!rules) {
        return;
    }
    memset(rules, 0, sizeof(*rules));
    if (!profile) {
        return;
    }
    rules->tile_bytes = profile->tile_bytes;
    rules->output_bytes = profile->output_bytes;
    rules->tile_count = mzg2_profile_tile_count(profile);
    rules->model_count = profile->model_count;
    for (model = 0u; model < profile->model_count; ++model) {
        rules->symbol_bits[model] = profile->model[model].symbol_bits;
    }
}

bool mzg2_profile_check(const mzg2_profile *profile, char *error,
                        size_t error_size)
{
    uint32_t model;
    uint32_t tiles;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (!profile) {
        mzg2_set_error(error, error_size, "no profile");
        return false;
    }
    if (profile->magic[0] == '\0') {
        mzg2_set_error(error, error_size, "profile magic is empty");
        return false;
    }
    if (!profile->sidecar_pattern || profile->sidecar_pattern[0] == '\0') {
        mzg2_set_error(error, error_size, "profile has no sidecar pattern");
        return false;
    }
    if (profile->tile_bytes == 0u ||
        profile->tile_bytes % MZG2_TILE_OUTPUT_GRANULE != 0u) {
        mzg2_set_error(error, error_size,
                       "tile bytes %u must be a nonzero multiple of %u",
                       profile->tile_bytes, MZG2_TILE_OUTPUT_GRANULE);
        return false;
    }
    if (profile->output_bytes == 0u ||
        profile->output_bytes % MZG2_TILE_OUTPUT_GRANULE != 0u) {
        mzg2_set_error(error, error_size,
                       "output bytes %u must be a nonzero multiple of %u",
                       profile->output_bytes, MZG2_TILE_OUTPUT_GRANULE);
        return false;
    }
    if (profile->max_layers == 0u || profile->max_layers > UINT16_MAX) {
        mzg2_set_error(error, error_size, "max layers %u out of range",
                       profile->max_layers);
        return false;
    }
    if (profile->experts_per_layer == 0u ||
        profile->experts_per_layer > UINT16_MAX) {
        mzg2_set_error(error, error_size, "experts per layer %u out of range",
                       profile->experts_per_layer);
        return false;
    }
    if (profile->model_count == 0u ||
        profile->model_count > MZG2_MAX_MODELS) {
        mzg2_set_error(error, error_size, "model count %u out of range",
                       profile->model_count);
        return false;
    }
    for (model = 0u; model < profile->model_count; ++model) {
        const uint8_t bits = profile->model[model].symbol_bits;
        uint32_t permitted = 0u;
        uint32_t value;

        if (bits != 4u && bits != 8u) {
            mzg2_set_error(error, error_size,
                           "model %u symbol bits %u is not 4 or 8", model,
                           bits);
            return false;
        }
        for (value = 0u; value < 256u; ++value) {
            if (mzg2_value_allowed(&profile->model[model], value)) {
                ++permitted;
            }
        }
        if (permitted == 0u) {
            mzg2_set_error(error, error_size,
                           "model %u permits no value", model);
            return false;
        }
    }
    tiles = mzg2_profile_tile_count(profile);
    if (tiles == 0u || tiles > UINT32_MAX / sizeof(mzg2_tile_descriptor)) {
        mzg2_set_error(error, error_size, "tile count %u out of range", tiles);
        return false;
    }
    /* The block header addresses regions with u32 fields, so the descriptor
     * table plus the fixed header and models must stay inside u32. */
    if ((uint64_t)sizeof(mzg2_block_header) + sizeof(mzg2_model_entry) +
            (uint64_t)tiles * sizeof(mzg2_tile_descriptor) >
        UINT32_MAX - (uint64_t)MZG2_ALIGNMENT) {
        mzg2_set_error(error, error_size,
                       "block metadata for %u tiles exceeds the 32-bit layout",
                       tiles);
        return false;
    }
    return true;
}
