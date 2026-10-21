/*
 * MZG2 model-independent core: profile description, reference codec, block
 * validation and store reading.
 *
 * The core never learns a model's identity. Everything model-specific --
 * magic string, decoded payload size, layer/expert bounds, which byte values
 * a symbol plane may carry, how wide a symbol is, the pinned source manifest,
 * sidecar naming -- arrives in an `mzg2_profile` built by an adapter
 * (adapters/<model>/).
 */
#ifndef MZG2_MZG2_H
#define MZG2_MZG2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mzg2/format.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- profile */

/*
 * A symbol plane's coding rules.
 *
 * `symbol_bits` is 4 (two symbols per byte, low nibble first) or 8 (one
 * symbol per byte). A v2 block does NOT carry this: the model tables describe
 * frequencies and values but not symbol width, so the profile supplies it.
 * See docs/format.md 13 -- moving it into block metadata is a candidate for a
 * future format revision.
 *
 * `allowed` is an optional 32-byte bitmap of permitted byte values (bit
 * `value & 7` of byte `value >> 3`); NULL permits every value representable
 * in `symbol_bits`. A profile uses it to express an alphabet restriction that
 * follows from its value transform -- for example a canonicalization that
 * removes one redundant code.
 */
typedef struct {
    uint8_t symbol_bits;
    const uint8_t *allowed;
} mzg2_model_spec;

typedef struct {
    /* Container identity. */
    char magic[8];                 /* file header magic, e.g. "K3MZG2\0" */
    const char *sidecar_pattern;   /* printf pattern taking one unsigned layer */

    /* Geometry. */
    uint32_t tile_bytes;           /* output bytes per full tile */
    uint32_t output_bytes;         /* decoded bytes per block */
    uint32_t max_layers;           /* valid layers are 1..max_layers */
    uint32_t experts_per_layer;    /* valid experts are 0..experts_per_layer-1 */

    /* Coding. */
    uint32_t model_count;          /* 1..MZG2_MAX_MODELS */
    mzg2_model_spec model[MZG2_MAX_MODELS];

    /*
     * Fail-closed source binding: when set, a sidecar whose
     * source_manifest_sha256 differs is rejected at open. NULL accepts any
     * digest, which is for synthetic fixtures, not for serving a model.
     */
    const uint8_t *source_manifest_sha256;

    /* A complete directory store must hold every layer and every expert. */
    bool require_complete_directory;
} mzg2_profile;

/* Validate internal consistency of a profile itself. */
bool mzg2_profile_check(const mzg2_profile *profile, char *error,
                        size_t error_size);

/* Tiles a block of this profile decodes into, and the output bytes of one. */
uint32_t mzg2_profile_tile_count(const mzg2_profile *profile);
uint32_t mzg2_profile_tile_output_bytes(const mzg2_profile *profile,
                                        uint32_t tile_index);

/*
 * Geometry the GPU decoder needs by value. Plain data: no pointers, so it can
 * cross into device code.
 */
typedef struct {
    uint32_t tile_bytes;
    uint32_t output_bytes;
    uint32_t tile_count;
    uint32_t model_count;
    uint8_t symbol_bits[MZG2_MAX_MODELS];
} mzg2_tile_rules;

void mzg2_profile_tile_rules(const mzg2_profile *profile,
                             mzg2_tile_rules *rules);

/* ------------------------------------------------------------------ models */

/*
 * A built model: the on-disk table plus the inverse map the encoder needs.
 */
typedef struct {
    mzg2_model table;
    uint8_t symbol_for_value[256]; /* 0xff where the value has no symbol */
} mzg2_built_model;

/*
 * Build a model from observed byte-value counts. The alphabet is every value
 * the spec permits (`spec->allowed`, and `< 2^symbol_bits`) with a nonzero
 * count, in ascending value order; frequencies are scaled to
 * MZG2_MODEL_SLOTS. Deterministic: the same counts always give the same
 * bytes, which is what makes a re-encode comparable to a stored block.
 */
bool mzg2_model_build(const uint64_t counts[256], const mzg2_model_spec *spec,
                      mzg2_built_model *out);

/* Validate a model table against a plane's rules (docs/format.md 7). */
bool mzg2_model_validate(const mzg2_model *model, const mzg2_model_spec *spec);

/* Validate every model a block carries. */
bool mzg2_model_entry_validate(const mzg2_model_entry *entry,
                               const mzg2_profile *profile);

/* --------------------------------------------------------- block structure */

/*
 * Resolved pointers into a validated block. Offsets are already bounds
 * checked, so a decoder can read through these without re-deriving them.
 */
typedef struct {
    const mzg2_block_header *header;
    const mzg2_model_entry *models;
    const mzg2_tile_descriptor *descriptors;
    uint32_t tile_count;
} mzg2_block_view;

/*
 * Host-side pre-decode gate: header fields, model tables and descriptor
 * bounds. Fails closed with a message. Both the CPU reference decoder and the
 * GPU launch path run this before touching a destination.
 *
 * `layer` and `expert` are the identity the caller asked for; the block must
 * agree with it. The block base must be at least 4-byte aligned so model
 * tables can be accessed safely; invalid profiles and bases are rejected.
 */
bool mzg2_block_validate(const mzg2_profile *profile, uint32_t layer,
                         uint32_t expert, const void *block,
                         uint32_t block_bytes, mzg2_block_view *view,
                         char *error, size_t error_size);

/* ------------------------------------------------------- reference codec */

/* A contiguous run of the decoded payload that is coded with one model. */
typedef struct {
    uint32_t offset;
    uint32_t bytes;
    uint32_t model;
} mzg2_plane;

/* An encoded block. Free with mzg2_encoded_release. */
typedef struct {
    uint8_t *bytes;
    uint32_t size;
    uint32_t tile_count;
    uint32_t raw_tiles; /* tiles that fell back to the raw escape */
} mzg2_encoded;

void mzg2_encoded_release(mzg2_encoded *encoded);

/*
 * Count symbol values over the planes and build one model per plane kind.
 * `source` is `profile->output_bytes` of decoded payload.
 */
bool mzg2_models_from_planes(const mzg2_profile *profile, const void *source,
                             const mzg2_plane *planes, uint32_t plane_count,
                             mzg2_built_model *models,
                             mzg2_model_entry *entry, char *error,
                             size_t error_size);

/*
 * Encode one block. The planes must tile the payload in output order: plane
 * boundaries must land on tile boundaries, and every tile's output bytes must
 * be a multiple of MZG2_TILE_OUTPUT_GRANULE.
 *
 * This is the reference encoder: portable C, no entropy-coding shortcuts, and
 * the byte layout the format specifies.
 */
bool mzg2_encode_block(const mzg2_profile *profile, uint32_t layer,
                       uint32_t expert, const void *source,
                       const mzg2_plane *planes, uint32_t plane_count,
                       const mzg2_built_model *models,
                       const mzg2_model_entry *entry, mzg2_encoded *out,
                       char *error, size_t error_size);

/*
 * CPU reference decoder. Writes exactly `profile->output_bytes` and returns
 * the format's error flags; on a nonzero return the destination is invalid
 * (docs/format.md 9). `error` receives a message for the first failure.
 * The block must be 4-byte aligned and must not overlap the destination.
 */
uint32_t mzg2_decode_block(const mzg2_profile *profile, uint32_t layer,
                           uint32_t expert, const void *block,
                           uint32_t block_bytes, void *destination,
                           uint32_t destination_bytes, char *error,
                           size_t error_size);

/* ------------------------------------------------------------------ store */

typedef struct mzg2_store mzg2_store;

/* Where a block lives: an O_DIRECT descriptor and an aligned extent. */
typedef struct {
    int direct_fd;
    uint64_t offset;
    uint32_t bytes;
} mzg2_span;

/*
 * Open a store: either one sidecar file (a partial layer, for tests) or a
 * directory of sidecars named by `profile->sidecar_pattern`. Validates every
 * header and index entry, rejects duplicate (layer, expert) pairs, and
 * enforces the profile's manifest binding and completeness rule.
 *
 * The path must be absolute. The profile must outlive the store.
 */
bool mzg2_store_open(const mzg2_profile *profile, const char *path,
                     mzg2_store **out, char *error, size_t error_size);

void mzg2_store_destroy(mzg2_store *store);

uint32_t mzg2_store_max_block_bytes(const mzg2_store *store);
uint64_t mzg2_store_present_count(const mzg2_store *store);

bool mzg2_store_span(const mzg2_store *store, uint32_t layer, uint32_t expert,
                     mzg2_span *span);

/*
 * Read one block into a caller buffer through the store's buffered
 * descriptor. For the serving path an engine reads the span itself with
 * O_DIRECT into GPU-mapped staging; this is the convenience path for tools
 * and tests.
 */
bool mzg2_store_read_block(const mzg2_store *store, uint32_t layer,
                           uint32_t expert, void *buffer, uint32_t buffer_bytes,
                           uint32_t *block_bytes, char *error,
                           size_t error_size);

#ifdef __cplusplus
}
#endif

#endif /* MZG2_MZG2_H */
