/*
 * Kimi K3 adapter: the qualified MZG2 research profile.
 *
 * Everything model-specific lives here -- the container magic, the 92x896
 * expert grid, the 17,547,264-byte decoded expert, the MXFP4 plane layout,
 * the zero-gauge value transform and the pinned source-manifest binding.
 * The core (mzg2/mzg2.h) knows none of it.
 *
 * Numerical contract: dequantized-value-preserving, exact against the
 * zero-gauge-canonical representation, NOT byte-exact to source MXFP4
 * negative zero (docs/format.md 12).
 */
#ifndef MZG2_ADAPTERS_KIMI_K3_H
#define MZG2_ADAPTERS_KIMI_K3_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mzg2/mzg2.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MZG2_K3_LAYERS = 92,
    MZG2_K3_EXPERTS_PER_LAYER = 896,
    MZG2_K3_EXPERT_BYTES = 17547264,
    MZG2_K3_TILE_BYTES = 16384,
    MZG2_K3_PACKED_BYTES = 5505024, /* MXFP4 nibble plane, per projection */
    MZG2_K3_SCALE_BYTES = 344064,   /* E8M0 scale plane, per projection */
    MZG2_K3_PLANES = 6,             /* w1, w2, w3 x (packed, scale) */
    MZG2_K3_MODEL_PACKED = 0,
    MZG2_K3_MODEL_SCALE = 1,
    /* The E2M1 nibble the zero-gauge transform removes. */
    MZG2_K3_NEGATIVE_ZERO_NIBBLE = 8
};

/*
 * The qualified profile, bound to the pinned K3 source manifest: a store
 * derived from any other source revision is rejected at open.
 */
const mzg2_profile *mzg2_kimi_k3_profile(void);

/*
 * The same geometry and coding rules with no source binding. For synthetic
 * fixtures and golden vectors, which have no model behind them. Never use it
 * to open a store that will serve a model.
 */
const mzg2_profile *mzg2_kimi_k3_profile_unbound(void);

/* The pinned source-manifest digest, as 32 bytes. */
const uint8_t *mzg2_kimi_k3_source_manifest_sha256(void);

/*
 * The six planes of a decoded expert, in payload order. Every plane is a
 * whole number of tiles, so plane boundaries land on tile boundaries.
 */
const mzg2_plane *mzg2_kimi_k3_planes(uint32_t *count);

/*
 * The source tensor name suffixes matching mzg2_kimi_k3_planes(), for a
 * transcoder that reads the official checkpoint. The adapter states the
 * names; reading SafeTensors is the caller's business.
 */
const char *const *mzg2_kimi_k3_plane_suffixes(uint32_t *count);

/*
 * Zero-gauge canonicalization of an MXFP4 nibble plane: fold E2M1 negative
 * zero onto positive zero, in place. Returns how many codes changed.
 *
 * This is why the K3 packed alphabet has 15 symbols rather than 16, and why
 * the contract is value-preserving rather than byte-exact. Apply it to every
 * packed plane before encoding, and to source bytes before comparing them
 * with a decode.
 */
uint64_t mzg2_kimi_k3_canonicalize_packed(void *data, uint32_t bytes);

/* Apply the canonicalization to every packed plane of a decoded expert. */
uint64_t mzg2_kimi_k3_canonicalize_expert(void *expert, uint32_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* MZG2_ADAPTERS_KIMI_K3_H */
