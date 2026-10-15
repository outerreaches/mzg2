# MZG2 format specification — draft v0.1 (K3 profile, format version 2)

Status: **pre-normative draft.** This document specifies the MZG2 v2 on-disk
format as produced and consumed by the qualified Kimi K3 implementation in the
Moonshine project (files `k3_mzg2.h` / `k3_mzg2.cu` and the transcode tools,
<https://github.com/outerreaches/moonshine>). It is published ahead of the
standalone codec extraction so the format can be reviewed; normative status,
golden vectors, and the CPU reference codec arrive with the extraction (see
`docs/roadmap.md`).

The format was designed for one job: stream MoE expert weights from storage and
decompress directly into an admitted destination (an expert-cache slot) on the
GPU, without a CPU decode step. Everything below is subordinate to that goal.

## 1. Scope

- **Specified here:** the sidecar/block container, the entropy-coding model
  tables, the tile structure, the decode procedure, integrity checks, and
  version/rejection behavior.
- **Not specified here (adapter or engine concerns):** where stores come from,
  how experts are admitted to a cache, I/O scheduling (queue depth, `O_DIRECT`
  staging), the distribution bundle container, and any model-specific value
  transform beyond the K3 profile's documented choices.
- **Reader/writer conformance:** a conforming reader must validate every
  structural invariant in this document before decoding and must fail closed
  (reject the block) if any check fails; a conforming writer must only produce
  blocks satisfying them. Corruption discovered during decode sets an error
  flag; the destination is invalid after any decode error (§9).

## 2. Conventions

- **Byte order:** little-endian throughout; all multi-byte integers LE.
- **Packing:** all structures are packed with no implicit padding (the
  reference implementation uses `#pragma pack(push, 1)`). Fixed ABI sizes:
  - file header: 124 bytes
  - index entry: 16 bytes
  - block header: 64 bytes
  - tile descriptor: 32 bytes
  - probability model: 4248 bytes (fixed size regardless of symbol count)
- **Offsets** are absolute within their containing file or block unless marked
  relative. All ranges are half-open `[start, start+len)` and must lie fully
  within their container; unsigned arithmetic must be overflow-checked
  (`len <= container_bytes - start`), never relied on wrap-around.
- **Alignment:** store alignment is 4096 bytes (direct-I/O friendly). Exact
  requirements are listed per field below. Padding bytes (alignment gaps,
  reserved regions) are **unspecified on read and must be ignored**; writers
  should zero them.
- **Reserved fields** must be zero unless defined; readers reject nonzero
  reserved fields.

## 3. Store layout

A store is either:

- a **directory** containing `layer-%03u.mzg2` sidecars for layers 1..92
  (K3 profile), each sidecar covering one full layer (896 experts), and the
  directory must be complete (all layers, all experts, no duplicates); or
- a **single sidecar file** covering a partial layer (a subset of experts of
  one layer), used for testing and partial deployment.

Each sidecar file contains, in order:

```
[ 0        , 4096 )  file header region (124-byte header + zero padding)
[ index_offset, +index_bytes )  expert index entries
[ data_offset, ... )  expert blocks, 4096-byte aligned, padded to 4096
```

`index_bytes = expert_count × 16`. `data_offset` may exceed the end of the
index region; the gap is padding.

## 4. File header (124 bytes)

| field | type | K3 v2 requirement |
|---|---|---|
| `magic` | `char[8]` | `"K3MZG2\0"` |
| `version` | `u32` | `2` |
| `header_bytes` | `u32` | `4096` |
| `alignment` | `u32` | `4096` |
| `layer` | `u32` | `1..92` |
| `expert_count` | `u32` | `1..896` |
| `index_entry_bytes` | `u32` | `16` |
| `model_entry_bytes` | `u32` | `0` (per-file models unused in this profile; models live in blocks) |
| `tile_bytes` | `u32` | `16384` |
| `flags` | `u32` | `0` |
| `index_offset` | `u64` | `4096` |
| `index_bytes` | `u64` | `expert_count × 16` |
| `model_offset` | `u64` | `0` |
| `model_bytes` | `u64` | `0` |
| `data_offset` | `u64` | `≥ index_offset + index_bytes`; multiple of 4096; ≤ file size |
| `max_block_bytes` | `u64` | nonzero; ≤ `UINT32_MAX`; multiple of 4096 |
| `source_manifest_sha256` | `u8[32]` | SHA-256 binding to the source manifest; the K3 profile **pins** this to the qualified value and rejects mismatches (fail-closed store binding) |

## 5. Expert index (16 bytes per entry)

| field | type | requirement |
|---|---|---|
| `layer` | `u16` | equals the file header's `layer` |
| `expert` | `u16` | `< 896` |
| `block_bytes` | `u32` | ≥ 1; multiple of 4096; ≤ `max_block_bytes` |
| `block_offset` | `u64` | multiple of 4096; ≥ `data_offset`; block fully within file |

An expert must appear at most once across the whole store; duplicated
`(layer, expert)` pairs are rejected. Entries may be in any order.

## 6. Expert block

Each block decodes to exactly one expert's routed tensor payload:
**17,547,264 bytes** (`EXPERT_BYTES`) for the K3 profile.

```
[ 0    , 64     )  block header (64 bytes)
[ model_offset, +8496 )  model entry (two probability models)
[ descriptor_offset, +tile_count×32 )  tile descriptors
[ payload_offset, ... )  tile payloads
[ payload end, block_bytes )  padding to a 4096-byte multiple
```

Offsets must be non-decreasing in the order header → models → descriptors →
payload; each region must lie within `block_bytes`; `model_offset` must be a
multiple of 4 and `descriptor_offset` a multiple of 8; `payload_offset` must be
a multiple of 4.

### 6.1 Block header (64 bytes)

| field | type | K3 v2 requirement |
|---|---|---|
| `magic` | `char[8]` | `"MZ2BLOCK"` |
| `version` | `u32` | `2` |
| `header_bytes` | `u32` | `64` |
| `layer` | `u32` | matches the request |
| `expert` | `u32` | matches the request |
| `tile_bytes` | `u32` | `16384` |
| `tile_count` | `u32` | `1071` (`EXPERT_BYTES / tile_bytes`) |
| `output_bytes` | `u32` | `17,547,264` |
| `descriptor_offset` | `u32` | ≥ `model_offset + model_bytes` |
| `descriptor_bytes` | `u32` | `tile_count × 32` |
| `payload_offset` | `u32` | ≥ `descriptor_offset + descriptor_bytes` |
| `block_bytes` | `u32` | equals the index entry's `block_bytes` |
| `flags` | `u32` | `0` |
| `model_offset` | `u32` | ≥ 64; multiple of 4 |
| `model_bytes` | `u32` | `8496` |

## 7. Probability models

A block carries one model entry: two static order-0 rANS models, indexed
`MODEL_PACKED = 0` (E2M1 nibble data) and `MODEL_SCALE = 1` (E8M0 scale
bytes). Each model is 4248 bytes:

| field | type | semantics |
|---|---|---|
| `decode[4096]` | `u8[4096]` | slot → symbol lookup table. `slot = state & 4095` (12-bit precision) |
| `values[15]` | `u8[15]` | symbol → value mapping (the transformed alphabet) |
| `symbol_count` | `u8` | 1..15 |
| `padding[16]` | `u8[16]` | unspecified |
| `frequency[15]` | `u32[15]` | per-symbol frequency |
| `cumulative[15]` | `u32[15]` | per-symbol cumulative offset |

Validation rules (rejected if any fails):

- `1 ≤ symbol_count ≤ 15`.
- Frequencies are nonzero; cumulatives start at 0 and accumulate exactly:
  `cumulative[s] == Σ_{t<s} frequency[t]`, final total `== 4096`.
- `values` are pairwise distinct. For the PACKED model every value is
  `≤ 15` and `≠ 8` (`8` is the E2M1 negative-zero nibble, removed by the K3
  adapter's zero-gauge canonicalization — this is why the symbol alphabet caps
  at 15 values). For the SCALE model values are unrestricted bytes.
- The decode table is consistent: for every slot, `decode[slot]` is
  `< symbol_count` and `cumulative[s] ≤ slot < cumulative[s] + frequency[s]`
  for `s = decode[slot]`.

Models are data, not code: a decoder needs no model-specific knowledge to
parse or validate them.

## 8. Tiles and decode

### 8.1 Tile descriptors (32 bytes, `tile_count` per block)

Descriptor `i` describes output tile `i`:

| field | type | requirement |
|---|---|---|
| `payload_offset` | `u32` | within block; multiple of 4 |
| `payload_bytes` | `u32` | within block |
| `output_offset` | `u32` | `== i × 16384` |
| `output_bytes` | `u32` | `== 16384` (K3 profile; all tiles full) |
| `kind` | `u32` | low byte = model index (0 or 1); bit 31 = raw escape; no other bits |
| `reserved` | `u32` | `0` |
| `checksum` | `u64` | low 32 bits carry the tile checksum (§8.4); high 32 bits zero |

Additional bounds: `output_offset + output_bytes ≤ EXPERT_BYTES`; payload
ranges within `block_bytes`; descriptor count equals `tile_count`.

### 8.2 Raw escape

If `kind & 0x80000000` is set, the payload is the tile's output **verbatim**
(`payload_bytes == output_bytes`). No entropy decoding; the checksum still
applies. The encoder falls back to raw whenever encoding would not shrink a
tile (incompressible regions cost an escape, not a larger block).

### 8.3 Encoded tile payload

For an encoded (non-raw) tile:

```
[ 0, 128 )           32 × u32 LE initial rANS states (one per lane, wave32)
[ 128, payload_bytes )  u16 LE word stream
```

Constraints: `payload_bytes ≥ 128`; `(payload_bytes − 128)` even.

Decode (per tile, one wave of 32 lanes; lane = 0..31):

1. `state[lane] = initial_states[lane]`; every state must be
   `≥ 65536` (the rANS lower bound) or the tile fails with a STATE error.
2. `rounds = output_bytes × symbols_per_byte / 32`, where
   `symbols_per_byte` is 2 for PACKED (two 4-bit symbols per byte) and 1 for
   SCALE. Each round, every lane decodes one symbol by the standard static
   rANS step: `slot = state & 4095`; `symbol = decode[slot]` (must be
   `< symbol_count`); `state = frequency[symbol] × (state >> 12) + slot −
   cumulative[symbol]`.
3. **Renormalization is wave-coalesced:** after the step, lanes whose state
   fell below 65536 are identified by a wave vote; the needing lanes consume
   the next `u16` words from the shared stream **in increasing lane order**
   (needing lane `l` takes the `popcount(mask & ((1 ≪ l) − 1))`-th word of
   this round's group) and `state = (state ≪ 16) | word`. The read cursor
   advances by the number of words consumed; consumption past the stream end
   fails with a STATE error. This interleaving is part of the format: writers
   emit the word stream in exactly the order these reads consume it.
4. **Output assembly (also format-defining):** PACKED: a lane accumulates 8
   decoded symbols into one output dword (nibble order: first symbol in the
   low nibble); every 8th round it writes dword index `(round ≫ 3) × 32 +
   lane`. SCALE: 4 symbols per dword; write index `(round ≫ 2) × 32 + lane`.
   Indices are output-dword units; the layout is lane-interleaved 128-byte
   groups.
5. **Terminal invariants (strict):** after all rounds, for every lane
   `cursor == word_count` **and** `state == 65536` exactly; otherwise the tile
   fails (TERMINAL error if the symbol stream decoded but the final
   bookkeeping is off; STATE error otherwise). Decoders must not be lenient:
   the final state must land exactly on the lower bound.

### 8.4 Tile checksum

Non-cryptographic, 32-bit, folded XOR over output dwords:

```
word_checksum(w, i) = rotl32(w, i mod 32) XOR (i × 0x9E3779B9)   (mod 2^32)
tile checksum       = XOR of word_checksum(dword, dword_index) over all output dwords
```

Each lane folds the dwords it wrote; the per-lane folds are reduced with an
XOR butterfly across the wave; the result must equal the low 32 bits of
`descriptor.checksum` or the tile fails with a CHECKSUM error. See
`docs/security.md` for what this checksum does and does not protect against.

## 9. Integrity and error reporting

Decoders report per-block error flags (bitwise OR, zero = success):

| flag | value | meaning |
|---|---|---|
| BOUNDS | 1 | geometry/descriptor/header violation (includes pre-decode validation failures) |
| STATE | 2 | rANS state violation: invalid initial state, invalid symbol, word exhaustion, decode-loop abort |
| CHECKSUM | 4 | tile checksum mismatch |
| TERMINAL | 8 | final cursor/state invariants violated after an otherwise valid decode |

Contract: **the destination is invalid after any decode error.** There is no
partial-success mode; callers must not publish decoded bytes when the error
flags are nonzero. Structural validation happens before decoding (host-side,
fail-closed, with a diagnostic message); the decode itself enforces the same
fail-closed policy at wave granularity.

## 10. Versioning and rejection

- `version` is `2` for this format. Readers reject any other version; there
  is no downgrade path in the decode direction.
- `flags` (file and block) and all reserved fields must be zero; nonzero =
  reject. No flag has been assigned yet; future flags will arrive with a
  version-negotiation story (a new version number, not silent flag adoption).
- Unknown `kind` bits, model indices beyond the defined set, or any field
  violating the tables above are rejection conditions, not warnings.
- Store-level: a missing, duplicated, or partially-invalid sidecar fails the
  whole store open (fail-closed selection); the raw SafeTensors path remains
  the fallback outside this format.

## 11. Profile constants (K3)

| constant | value |
|---|---|
| layers / experts per layer | 92 / 896 |
| expert bytes | 17,547,264 |
| tile bytes | 16,384 |
| tiles per expert | 1071 |
| store alignment | 4096 |
| rANS precision | 12 bits (slot = state & 4095; total = 4096) |
| rANS lower bound | 65,536 (2^16) |
| wave size / workgroup | 32 / 256 |
| sidecar naming | `layer-%03u.mzg2`, layers 1..92 |
| source binding | `source_manifest_sha256` pinned to the qualified K3 source manifest |

Measured reference results for this profile (hardware-bounded, single
qualification): routed-expert storage traffic reduced 13.0674%
(1,446,456,066,048 → 1,257,441,714,176 bytes across the full store);
decoder output 26.5–28.2 GiB/s under concurrent expert GEMM; end-to-end
fixtures improved ~10.6–12.8% on the qualified stack. Boundaries and full
evidence live in the project's qualification records.

## 12. What "exact" means here

The transform is **dequantized-value-preserving** and exact against its
zero-gauge-canonical representation: decoding reproduces the adapter's
canonicalized expert values exactly (verified by a full-store CPU roundtrip
and an independent second-pass comparison against the source SafeTensors,
plus exact generated tokens in engine gates). It is **not** byte-exact to
source MXFP4: negative-zero gauge choices are canonicalized (E2M1 `−0` is
folded before encoding), so bytes fed in and their provably equivalent
canonical forms may differ bit-wise. This is a deliberate, documented trade.

## 13. To normative status

Before this document becomes normative (planned with the standalone codec
extraction), it needs: golden byte-level vectors and a model-free CPU
reference decoder proving they decode; a frozen cross-ISA statement (wave32
required in v2; wave64/discrete-GPU backends unspecified); a formalized
version-evolution policy; a decision on which currently-K3-profile semantics
(two fixed plane models, tile geometry) move to adapter metadata in a future
format revision; and resolution of the remaining publication-checklist items
recorded in the project vault.
