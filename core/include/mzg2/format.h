/*
 * MZG2 on-disk format, version 2 (see docs/format.md).
 *
 * This header is the ABI only: container structures, fixed sizes, symbol
 * limits and error flags. It carries no model-specific constant -- no layer
 * count, expert count, tensor name, payload size or manifest digest. Those
 * belong to a profile (mzg2/mzg2.h) supplied by an adapter.
 */
#ifndef MZG2_FORMAT_H
#define MZG2_FORMAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Format version implemented here. Readers reject every other value. */
    MZG2_VERSION = 2,

    /* Container geometry. */
    MZG2_FILE_HEADER_BYTES = 4096,
    MZG2_ALIGNMENT = 4096,

    /* Entropy coder. The wave32 lane count is format-defining in v2: the
     * word stream is emitted in exactly the order 32 lanes consume it. */
    MZG2_MODEL_SLOTS = 4096, /* rANS precision 12 bits; frequencies sum here */
    MZG2_SCALE_BITS = 12,
    MZG2_LOWER_BOUND = 1u << 16,
    MZG2_LANES = 32,
    MZG2_MAX_SYMBOLS = 15,

    /* Models carried per block in v2: one per symbol plane kind. */
    MZG2_MAX_MODELS = 2,

    /* Output bytes a single tile must be a multiple of: a lane assembles
     * whole dwords and writes them in 128-byte lane-interleaved groups. */
    MZG2_TILE_OUTPUT_GRANULE = 128,

    /* Tile descriptor `kind` field: low byte selects the model. */
    MZG2_MODEL_MASK = 0xffu,

    /* Decode error flags, OR-ed per block; zero means success. */
    MZG2_ERROR_BOUNDS = 1u,
    MZG2_ERROR_STATE = 2u,
    MZG2_ERROR_CHECKSUM = 4u,
    MZG2_ERROR_TERMINAL = 8u
};

/* Tile descriptor `kind` bit 31: the payload is the tile's output verbatim.
 * Not an enumerator, because it does not fit a signed int. */
#define MZG2_TILE_RAW 0x80000000u

#pragma pack(push, 1)

/* Sidecar file header: 124 bytes, followed by zero padding to 4096. */
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t header_bytes;
    uint32_t alignment;
    uint32_t layer;
    uint32_t expert_count;
    uint32_t index_entry_bytes;
    uint32_t model_entry_bytes;
    uint32_t tile_bytes;
    uint32_t flags;
    uint64_t index_offset;
    uint64_t index_bytes;
    uint64_t model_offset;
    uint64_t model_bytes;
    uint64_t data_offset;
    uint64_t max_block_bytes;
    uint8_t source_manifest_sha256[32];
} mzg2_file_header;

/* Expert index entry: 16 bytes. */
typedef struct {
    uint16_t layer;
    uint16_t expert;
    uint32_t block_bytes;
    uint64_t block_offset;
} mzg2_index_entry;

/* Block header: 64 bytes. */
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t header_bytes;
    uint32_t layer;
    uint32_t expert;
    uint32_t tile_bytes;
    uint32_t tile_count;
    uint32_t output_bytes;
    uint32_t descriptor_offset;
    uint32_t descriptor_bytes;
    uint32_t payload_offset;
    uint32_t block_bytes;
    uint32_t flags;
    uint32_t model_offset;
    uint32_t model_bytes;
} mzg2_block_header;

/* Tile descriptor: 32 bytes, tile_count per block. */
typedef struct {
    uint32_t payload_offset;
    uint32_t payload_bytes;
    uint32_t output_offset;
    uint32_t output_bytes;
    uint32_t kind;
    uint32_t reserved;
    uint64_t checksum;
} mzg2_tile_descriptor;

#pragma pack(pop)

/*
 * Static order-0 rANS model: 4248 bytes. Deliberately not packed -- the u32
 * arrays follow 4128 bytes of u8, so the natural layout has no padding and
 * alignment stays 4, which is what the block header's model_offset rule
 * requires.
 */
typedef struct {
    uint8_t decode[MZG2_MODEL_SLOTS]; /* slot -> symbol; slot = state & 4095 */
    uint8_t values[MZG2_MAX_SYMBOLS]; /* symbol -> value */
    uint8_t symbol_count;
    uint8_t padding[16]; /* unspecified on read */
    uint32_t frequency[MZG2_MAX_SYMBOLS];
    uint32_t cumulative[MZG2_MAX_SYMBOLS];
} mzg2_model;

/* One model entry per block: 8496 bytes. */
typedef struct {
    mzg2_model model[MZG2_MAX_MODELS];
} mzg2_model_entry;

/* Magic strings. The file magic is profile-supplied; the block magic is
 * format-wide. */
#define MZG2_BLOCK_MAGIC "MZ2BLOCK"

/*
 * Tile checksum (docs/format.md 8.4): non-cryptographic 32-bit folded XOR
 * over output dwords. Shared by the encoder, the CPU reference decoder and
 * the GPU decoder, so the definition lives in the header.
 */
#if defined(__HIPCC__) || defined(__CUDACC__)
#define MZG2_CHECKSUM_QUALIFIER __host__ __device__ static inline
#else
#define MZG2_CHECKSUM_QUALIFIER static inline
#endif

MZG2_CHECKSUM_QUALIFIER uint32_t mzg2_word_checksum(uint32_t word,
                                                    uint32_t dword_index)
{
    const uint32_t rotation = dword_index & 31u;
    const uint32_t rotated =
        rotation ? (word << rotation) | (word >> (32u - rotation)) : word;
    return rotated ^ (dword_index * (uint32_t)0x9e3779b9u);
}

#ifdef __cplusplus
}
#endif

#endif /* MZG2_FORMAT_H */
