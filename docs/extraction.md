# Extraction record — October 2026

What moved out of the qualified Kimi K3 implementation into this repository,
what changed on the way, what the extraction found, and what evidence backs
it.

The guiding rule was the one in `CONTRIBUTING.md`: the core never learns a
model's identity. Everything K3-specific is in `adapters/kimi-k3/`.

## What landed

| Area | Files |
|---|---|
| Format ABI | `core/include/mzg2/format.h` |
| Profile, codec and store API | `core/include/mzg2/mzg2.h` |
| Profile geometry | `core/src/profile.c` |
| Probability models | `core/src/model.c` |
| Pre-decode validation | `core/src/block.c` |
| CPU reference encoder/decoder | `core/src/codec.c` |
| Store reader | `core/src/store.c` |
| HIP wave32 decoder | `core/src/decode_hip.hip`, `core/include/mzg2/decode_hip.h` |
| Kimi K3 adapter | `adapters/kimi-k3/` |
| Tests, fixtures, golden vectors | `tests/` |
| Store checker, vector generator | `tools/` |

The CPU reference codec is new as a published artifact, but not new code: the
qualified transcoder already carried a scalar encoder and a scalar decoder it
used to verify every expert it wrote. Those are what became
`mzg2_encode_block` and `mzg2_decode_block`, with the model constants
replaced by a profile.

## The core/adapter boundary

Seven things were K3 constants in the qualified implementation and are
profile fields now:

| Was | Is |
|---|---|
| `"K3MZG2\0"` | `profile->magic` |
| 92 layers, 896 experts | `max_layers`, `experts_per_layer` |
| `K3_MZG2_EXPERT_BYTES` 17,547,264 | `output_bytes` |
| tile size 16,384 hard-checked | `tile_bytes` |
| pinned manifest digest in the decoder | `source_manifest_sha256`, or NULL |
| `layer-%03u.mzg2` | `sidecar_pattern` |
| `MOONSHINE_MZG2_STORE` | not the core's business: it takes a path |

Two were *implicit* in the K3 code and are now explicit, which is where the
extraction did real design work rather than moving text:

- **Symbol width.** The kernel decided "two symbols per byte" by testing
  `model == MODEL_PACKED`. The model index is a plane kind, not a width, so
  the core takes `symbol_bits` (4 or 8) per model from the profile. A v2
  block does not carry it — see the finding below.
- **Alphabet restriction.** The validator special-cased "packed values must
  be `<= 15` and `!= 8`", which is MXFP4 zero-gauge knowledge. The core takes
  a 32-byte bitmap of permitted values per model; the K3 adapter supplies the
  bitmap that removes the negative-zero nibble, and `tests/fixture.h`
  supplies the same shape for a model that has nothing to do with MXFP4.

## Findings

### A v2 block does not describe its own symbol width

The model tables carry frequencies, cumulative offsets, a decode table and
the value alphabet — everything except how many bits a symbol occupies. A
decoder that knows only the bitstream therefore cannot decode a block: it
needs the profile to tell it the width of each plane kind. For a single-model
format that would be harmless; with two plane kinds it means the container is
not self-describing.

This is recorded, not fixed: changing it is a format revision, and v2 is what
the qualified store on disk uses. `docs/format.md` §13 lists it as a
candidate for the next revision, and §8.3 now says plainly where the width
comes from.

### Two specification rules were stricter than the qualified reader

Both are satisfied by every block that reader ever wrote, so the core
enforces the specification:

- **`descriptor_offset` must be a multiple of 8.** The K3 reader wrote the
  check as `% alignof(tile_descriptor)`. The descriptor is a packed struct,
  so its alignment is 1 and the check was vacuous. The encoder places the
  table at 8,560, which is a multiple of 8.
- **A descriptor's `checksum` field must have its high 32 bits zero.** The
  reader compared only the low word. The encoder assigns a `uint32_t`, so the
  high word is always zero.

Enforcing them is a narrowing: a block the old reader accepted could now be
rejected. None of the 14 sampled experts violated these rules; the qualified
writer's layout and checksum assignments satisfy both. The sample does not
constitute a new exhaustive scan of the full store.

### A restricted alphabet compresses very differently per symbol width

Fifteen equiprobable values need 3.91 bits. In an 8-bit plane that is a ~51%
saving; in a 4-bit plane it is a ~2% saving, which the 128-byte initial-state
header then wipes out, so the tile escapes to raw. The `fixture-uniform`
golden vector pins both halves of that behaviour in one block.

## Evidence

**Model-free tests.** Clean build with `-std=c11 -Wall -Wextra -Wpedantic
-Werror`; **316 checks, 0 failures** across five binaries:

| Binary | Checks | Covers |
|---|---:|---|
| `test_format` | 99 | ABI sizes, profile rules, model build/validate, adapter geometry and the zero-gauge transform |
| `test_codec` | 69 | roundtrip, both tile paths, determinism, re-encode fixed point, K3 geometry on a synthetic payload |
| `test_malformed` | 45 | one injected defect per rule: header, model table, descriptor, integrity, terminal invariants |
| `test_store` | 59 | span/read path plus 30 rejection cases on a store the test builds |
| `test_vectors` | 44 | the committed golden vectors decode, match the catalogue, and re-encode byte-identically |

Every rejection case starts from a block or store that passes and changes one
thing, so each gate has been seen to fail. A validator that has never failed
is not evidence.

**Against the real qualified store.** `tools/mzg2_store_check` with
`--reencode` read 14 experts from the full K3 derived store — layer 1 experts
0–2 and 100–101 through a single sidecar, then experts 440–442 of layers 1,
46 and 92 through the directory store — and for every one:

- the store opened under the **source-bound** K3 profile, so the pinned
  manifest digest matched;
- the CPU reference decoder returned no error flags, meaning all 1,071 tile
  checksums and terminal invariants held;
- **`encode(decode(block))` was byte-identical to the stored block.**

That last line is the extraction's byte-level claim. A decoder alone proves a
block parses; a matching re-encode proves the extracted encoder's entropy
model, frequency quantization with its tie-breaks, lane interleave and block
layout are the ones that produced the sampled store blocks. It needs no
additional source checkpoint or GPU; access to the derived weights remains
subject to their original model license.

Sampled reduction was 12.838%–13.111% per sampled group, bracketing the
recorded full-store figure of 13.0674%.

## Not yet done

- **The HIP decoder has not been run.** It compiles clean for `gfx1151`
  (`make hip`, `-Werror`), and it is a direct port with the same arithmetic,
  but no kernel was launched during this extraction: the only wave32 hardware
  here was serving a model. Until a GPU window, treat the kernel as
  reviewed-not-executed. The gate is full-block agreement with the CPU
  reference on the golden vectors plus real store blocks, with the error
  flags exercised by the same injected defects `test_malformed` uses.
- **No store writer is published.** Building a K3 store still means the
  Moonshine transcoder; `tests/sidecar.h` writes sidecars for tests only.
- **wave64 is unspecified.** The v2 renormalization order is defined by 32
  lanes; a wave64 target needs its own kernel.
- **Fuzzing.** The validation surface is covered by hand-written cases, not
  by a fuzzer.
- **A second adapter** remains the open question the project's reuse claim
  depends on (roadmap Phase 4).
