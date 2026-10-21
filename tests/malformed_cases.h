/*
 * Fail-closed behaviour: every structural rule, the integrity checks, and
 * the terminal invariants, each proved to reject by re-injecting exactly one
 * defect into an otherwise valid block.
 *
 * A validation gate that has never been seen to fail is not evidence of
 * anything, so each case below starts from a block that passes and changes
 * one thing.
 */
#ifndef MZG2_TESTS_MALFORMED_CASES_H
#define MZG2_TESTS_MALFORMED_CASES_H

#include "decoder_cases.h"
#include "fixture.h"
#include "harness.h"
#include "mzg2/mzg2.h"

static mzg2_test_decode_fn mzg2_malformed_decode;

typedef struct {
    uint8_t *bytes;
    uint32_t size;
    uint8_t payload[MZG2_FIXTURE_OUTPUT_BYTES];
} baseline;

static bool build_baseline(baseline *out)
{
    mzg2_encoded encoded;
    char error[256];

    mzg2_fixture_fill(out->payload, MZG2_FIXTURE_SEED + 7u);
    if (!mzg2_fixture_encode(out->payload, 1u, 0u, &encoded, error,
                             sizeof(error))) {
        printf("  baseline encode failed: %s\n", error);
        return false;
    }
    out->bytes = encoded.bytes;
    out->size = encoded.size;
    return true;
}

/* Decode a mutated copy and return the error flags. */
static uint32_t decode_copy(const baseline *base, uint8_t *copy,
                            uint32_t size)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    uint8_t decoded[MZG2_FIXTURE_OUTPUT_BYTES];
    char error[256];

    uint32_t flags;
    memset(decoded, 0, sizeof(decoded));
    flags = mzg2_malformed_decode(profile, 1u, 0u, copy, size, decoded,
                                  sizeof(decoded), error, sizeof(error));
    if (copy == base->bytes && flags == 0u) {
        CHECK(memcmp(decoded, base->payload, sizeof(decoded)) == 0);
    }
    return flags;
}

#define MUTATE(label, expected_flags, statements)                             \
    do {                                                                      \
        uint8_t *copy = (uint8_t *)malloc(base->size);                        \
        uint32_t size = base->size;                                           \
        mzg2_block_header *header;                                            \
        mzg2_tile_descriptor *descriptors;                                    \
        uint32_t flags;                                                       \
                                                                              \
        CHECK(copy != NULL);                                                  \
        if (!copy) {                                                          \
            break;                                                            \
        }                                                                     \
        memcpy(copy, base->bytes, base->size);                                \
        header = (mzg2_block_header *)copy;                                   \
        descriptors =                                                         \
            (mzg2_tile_descriptor *)(copy + header->descriptor_offset);       \
        (void)descriptors;                                                    \
        statements;                                                           \
        flags = decode_copy(base, copy, size);                                \
        if ((expected_flags) == 0xffffffffu) {                                \
            CHECK_MSG(flags != 0u, "%s was accepted", label);                 \
        } else {                                                              \
            CHECK_MSG(flags == (expected_flags), "%s gave flags %u", label,   \
                      flags);                                                 \
        }                                                                     \
        free(copy);                                                           \
        CHECK(decode_copy(base, base->bytes, base->size) == 0u);                                                           \
    } while (0)

static void test_header_rules(const baseline *base)
{
    printf("header rules\n");
    MUTATE("block magic", MZG2_ERROR_BOUNDS, header->magic[0] = 'X');
    MUTATE("version", MZG2_ERROR_BOUNDS, header->version = MZG2_VERSION + 1u);
    MUTATE("version 1 (no downgrade)", MZG2_ERROR_BOUNDS, header->version = 1u);
    MUTATE("header_bytes", MZG2_ERROR_BOUNDS, header->header_bytes = 60u);
    MUTATE("nonzero flags", MZG2_ERROR_BOUNDS, header->flags = 1u);
    MUTATE("tile_bytes", MZG2_ERROR_BOUNDS, header->tile_bytes = 512u);
    MUTATE("tile_count", MZG2_ERROR_BOUNDS, header->tile_count = 7u);
    MUTATE("output_bytes", MZG2_ERROR_BOUNDS, header->output_bytes = 4096u);
    MUTATE("block_bytes disagrees with the read size", MZG2_ERROR_BOUNDS,
           header->block_bytes = base->size - MZG2_ALIGNMENT);
    MUTATE("model_offset misaligned", MZG2_ERROR_BOUNDS,
           header->model_offset += 1u);
    MUTATE("model_offset inside the header", MZG2_ERROR_BOUNDS,
           header->model_offset = 32u);
    MUTATE("model_bytes", MZG2_ERROR_BOUNDS, header->model_bytes = 4248u);
    MUTATE("descriptor_offset misaligned", MZG2_ERROR_BOUNDS,
           header->descriptor_offset += 4u);
    MUTATE("descriptor_offset overlaps the models", MZG2_ERROR_BOUNDS,
           header->descriptor_offset = 64u);
    MUTATE("descriptor_bytes", MZG2_ERROR_BOUNDS,
           header->descriptor_bytes -= 32u);
    MUTATE("descriptor table past the block", MZG2_ERROR_BOUNDS,
           header->descriptor_offset = base->size - 64u);
    MUTATE("payload_offset overlaps the descriptors", MZG2_ERROR_BOUNDS,
           header->payload_offset = header->descriptor_offset);
    MUTATE("payload_offset misaligned", MZG2_ERROR_BOUNDS,
           header->payload_offset += 1u);
    MUTATE("payload_offset past the block", MZG2_ERROR_BOUNDS,
           header->payload_offset = base->size + 4096u);
    /* A short read must be refused, not decoded from a truncated buffer. */
    MUTATE("truncated block", MZG2_ERROR_BOUNDS, size = base->size / 2u);
    MUTATE("header-only block", MZG2_ERROR_BOUNDS, size = 32u);
}

static void test_model_table_rules(const baseline *base)
{
    printf("model table rules\n");
    MUTATE("model frequency", MZG2_ERROR_BOUNDS, {
        mzg2_model_entry *models =
            (mzg2_model_entry *)(copy + header->model_offset);
        models->model[0].frequency[0] += 1u;
    });
    MUTATE("model cumulative", MZG2_ERROR_BOUNDS, {
        mzg2_model_entry *models =
            (mzg2_model_entry *)(copy + header->model_offset);
        models->model[0].cumulative[1] += 1u;
    });
    MUTATE("model symbol count", MZG2_ERROR_BOUNDS, {
        mzg2_model_entry *models =
            (mzg2_model_entry *)(copy + header->model_offset);
        models->model[1].symbol_count = 0u;
    });
    MUTATE("model decode table", MZG2_ERROR_BOUNDS, {
        mzg2_model_entry *models =
            (mzg2_model_entry *)(copy + header->model_offset);
        models->model[0].decode[0] =
            (uint8_t)(models->model[0].symbol_count - 1u);
    });
    /* A value outside the plane's alphabet is rejected even though the
     * table is otherwise consistent: this is the restricted-alphabet rule
     * the profile supplies. */
    MUTATE("model value outside the alphabet", MZG2_ERROR_BOUNDS, {
        mzg2_model_entry *models =
            (mzg2_model_entry *)(copy + header->model_offset);
        uint32_t symbol;

        for (symbol = 0u; symbol < models->model[0].symbol_count; ++symbol) {
            if (models->model[0].values[symbol] == 15u) {
                models->model[0].values[symbol] = 8u;
                break;
            }
        }
    });
}

static void test_descriptor_rules(const baseline *base)
{
    printf("descriptor rules\n");
    MUTATE("unknown kind bit", MZG2_ERROR_BOUNDS,
           descriptors[0].kind |= 0x100u);
    MUTATE("model index past the profile", MZG2_ERROR_BOUNDS, {
        descriptors[0].kind =
            (descriptors[0].kind & ~MZG2_MODEL_MASK) | 2u;
    });
    MUTATE("reserved field", MZG2_ERROR_BOUNDS, descriptors[1].reserved = 1u);
    MUTATE("checksum high word", MZG2_ERROR_BOUNDS,
           descriptors[2].checksum |= (uint64_t)1u << 32u);
    MUTATE("output_offset out of order", MZG2_ERROR_BOUNDS,
           descriptors[3].output_offset += MZG2_FIXTURE_TILE_BYTES);
    MUTATE("output_bytes", MZG2_ERROR_BOUNDS,
           descriptors[3].output_bytes = MZG2_FIXTURE_TILE_BYTES / 2u);
    MUTATE("payload_offset misaligned", MZG2_ERROR_BOUNDS,
           descriptors[0].payload_offset += 1u);
    MUTATE("payload past the block", MZG2_ERROR_BOUNDS,
           descriptors[0].payload_bytes = base->size);
}

static void find_tiles(const baseline *base, uint32_t *coded, uint32_t *raw)
{
    const mzg2_profile *profile = mzg2_fixture_profile();
    mzg2_block_view view;
    char error[256];
    uint32_t tile;

    *coded = UINT32_MAX;
    *raw = UINT32_MAX;
    if (!mzg2_block_validate(profile, 1u, 0u, base->bytes, base->size, &view,
                             error, sizeof(error))) {
        return;
    }
    for (tile = 0u; tile < view.tile_count; ++tile) {
        if (view.descriptors[tile].kind & MZG2_TILE_RAW) {
            if (*raw == UINT32_MAX) {
                *raw = tile;
            }
        } else if (*coded == UINT32_MAX) {
            *coded = tile;
        }
    }
}

static void test_integrity(const baseline *base)
{
    uint32_t coded = UINT32_MAX;
    uint32_t raw = UINT32_MAX;

    printf("integrity and terminal invariants\n");
    find_tiles(base, &coded, &raw);
    CHECK(coded != UINT32_MAX);
    CHECK(raw != UINT32_MAX);
    if (coded == UINT32_MAX || raw == UINT32_MAX) {
        return;
    }

    /* A flipped bit in a raw payload is caught by the tile checksum. */
    MUTATE("raw payload bit flip", MZG2_ERROR_CHECKSUM,
           copy[descriptors[raw].payload_offset + 7u] ^= 0x01u);

    /* A flipped bit in a coded payload may desynchronize the state machine
     * or survive to the checksum; either way it must be rejected. */
    MUTATE("coded payload bit flip", 0xffffffffu,
           copy[descriptors[coded].payload_offset + 200u] ^= 0x01u);

    /* An initial state below the rANS lower bound. */
    MUTATE("initial state below the lower bound", MZG2_ERROR_STATE, {
        uint32_t zero = 0u;
        memcpy(copy + descriptors[coded].payload_offset, &zero,
               sizeof(zero));
    });

    /* A short word stream: the decoder must run out, not read past it. */
    MUTATE("word stream shortened", 0xffffffffu,
           descriptors[coded].payload_bytes -= 2u);

    /* Extra words left unread break the terminal invariant. */
    MUTATE("word stream extended", MZG2_ERROR_TERMINAL,
           descriptors[coded].payload_bytes += 2u);

    MUTATE("stored checksum tampered", MZG2_ERROR_CHECKSUM,
           descriptors[raw].checksum ^= 0xffu);

    /* A coded tile relabelled raw: its payload is shorter than a tile. */
    MUTATE("coded tile relabelled raw", MZG2_ERROR_BOUNDS,
           descriptors[coded].kind |= MZG2_TILE_RAW);

    /* A raw tile relabelled coded: its bytes are not a rANS stream. */
    MUTATE("raw tile relabelled coded", 0xffffffffu,
           descriptors[raw].kind &= ~MZG2_TILE_RAW);

    /* Two tiles pointing at the same payload: the second checksum fails. */
    MUTATE("payload aliased to another tile", 0xffffffffu, {
        descriptors[raw].payload_offset =
            descriptors[raw == 0u ? 1u : 0u].payload_offset;
    });
}

static int mzg2_run_malformed(mzg2_test_decode_fn decode)
{
    baseline base;
    baseline *pointer = &base;

    mzg2_malformed_decode = decode;
    memset(&base, 0, sizeof(base));
    if (!build_baseline(&base)) {
        CHECK(false);
        return 1;
    }
    /* The unmutated block must decode, or nothing below means anything. */
    CHECK(decode_copy(pointer, base.bytes, base.size) == 0u);
    test_header_rules(pointer);
    test_model_table_rules(pointer);
    test_descriptor_rules(pointer);
    test_integrity(pointer);
    free(base.bytes);
    return mzg2_test_report("test_malformed");
}

#endif /* MZG2_TESTS_MALFORMED_CASES_H */
