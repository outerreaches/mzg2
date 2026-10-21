/*
 * Static order-0 rANS models: construction from observed counts, and the
 * reader-side validation rules of docs/format.md 7.
 *
 * The construction arithmetic is the extraction of the qualified Kimi K3
 * transcoder's, unchanged, because the resulting table bytes are part of a
 * stored block: a re-encode can only reproduce a stored block byte for byte
 * if the frequency quantization and its tie-breaks are identical.
 */
#include "mzg2/mzg2.h"

#include <string.h>

#include "internal.h"

bool mzg2_model_validate(const mzg2_model *model, const mzg2_model_spec *spec)
{
    uint32_t count;
    uint32_t total = 0u;
    uint32_t symbol;
    uint32_t slot;

    if (!model || !spec) {
        return false;
    }
    count = model->symbol_count;
    if (count == 0u || count > MZG2_MAX_SYMBOLS) {
        return false;
    }
    for (symbol = 0u; symbol < count; ++symbol) {
        const uint32_t frequency = model->frequency[symbol];
        const uint8_t value = model->values[symbol];
        uint32_t previous;

        if (model->cumulative[symbol] != total || frequency == 0u ||
            frequency > MZG2_MODEL_SLOTS - total) {
            return false;
        }
        if (!mzg2_value_allowed(spec, value)) {
            return false;
        }
        for (previous = 0u; previous < symbol; ++previous) {
            if (model->values[previous] == value) {
                return false;
            }
        }
        total += frequency;
    }
    if (total != MZG2_MODEL_SLOTS) {
        return false;
    }
    for (slot = 0u; slot < MZG2_MODEL_SLOTS; ++slot) {
        const uint32_t decoded = model->decode[slot];

        if (decoded >= count || slot < model->cumulative[decoded] ||
            slot >= model->cumulative[decoded] + model->frequency[decoded]) {
            return false;
        }
    }
    return true;
}

bool mzg2_model_entry_validate(const mzg2_model_entry *entry,
                               const mzg2_profile *profile)
{
    uint32_t model;

    if (!entry || !profile) {
        return false;
    }
    for (model = 0u; model < profile->model_count; ++model) {
        if (!mzg2_model_validate(&entry->model[model],
                                 &profile->model[model])) {
            return false;
        }
    }
    return true;
}

bool mzg2_model_build(const uint64_t counts[256], const mzg2_model_spec *spec,
                      mzg2_built_model *out)
{
    uint8_t values[MZG2_MAX_SYMBOLS];
    uint64_t remainder[MZG2_MAX_SYMBOLS];
    uint32_t value_count = 0u;
    uint64_t total = 0u;
    uint32_t frequency_sum = 0u;
    uint32_t cumulative = 0u;
    uint32_t symbol;
    uint32_t value;

    if (!counts || !spec || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    memset(out->symbol_for_value, 0xff, sizeof(out->symbol_for_value));

    /* Alphabet: permitted values with a nonzero count, ascending. */
    for (value = 0u; value < 256u; ++value) {
        if (counts[value] == 0u || !mzg2_value_allowed(spec, value)) {
            continue;
        }
        if (value_count >= MZG2_MAX_SYMBOLS) {
            return false;
        }
        values[value_count++] = (uint8_t)value;
        total += counts[value];
    }
    if (value_count == 0u || total == 0u ||
        total > UINT64_MAX / MZG2_MODEL_SLOTS) {
        return false;
    }

    for (symbol = 0u; symbol < value_count; ++symbol) {
        const uint8_t symbol_value = values[symbol];
        uint32_t frequency =
            (uint32_t)(counts[symbol_value] * MZG2_MODEL_SLOTS / total);

        if (frequency == 0u) {
            frequency = 1u;
        }
        out->table.frequency[symbol] = frequency;
        remainder[symbol] = counts[symbol_value] * MZG2_MODEL_SLOTS % total;
        frequency_sum += frequency;
        out->table.values[symbol] = symbol_value;
        out->symbol_for_value[symbol_value] = (uint8_t)symbol;
    }

    /* Hand the shortfall to the largest remainders, at most one each. */
    while (frequency_sum < MZG2_MODEL_SLOTS) {
        uint32_t best = 0u;

        for (symbol = 1u; symbol < value_count; ++symbol) {
            if (remainder[symbol] > remainder[best]) {
                best = symbol;
            }
        }
        ++out->table.frequency[best];
        remainder[best] = 0u;
        ++frequency_sum;
    }
    /* Take an overshoot from the smallest remainders, never below one. */
    while (frequency_sum > MZG2_MODEL_SLOTS) {
        uint32_t best = UINT32_MAX;

        for (symbol = 0u; symbol < value_count; ++symbol) {
            if (out->table.frequency[symbol] > 1u &&
                (best == UINT32_MAX ||
                 remainder[symbol] < remainder[best])) {
                best = symbol;
            }
        }
        if (best == UINT32_MAX) {
            return false;
        }
        --out->table.frequency[best];
        --frequency_sum;
    }
    if (frequency_sum != MZG2_MODEL_SLOTS) {
        return false;
    }

    for (symbol = 0u; symbol < value_count; ++symbol) {
        const uint32_t end = cumulative + out->table.frequency[symbol];

        if (end > MZG2_MODEL_SLOTS) {
            return false;
        }
        out->table.cumulative[symbol] = cumulative;
        memset(out->table.decode + cumulative, (int)symbol, end - cumulative);
        cumulative = end;
    }
    out->table.symbol_count = (uint8_t)value_count;
    return cumulative == MZG2_MODEL_SLOTS;
}
