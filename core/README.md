# Core

The model-independent format, reference codec, store reader and HIP decoder.

It must not contain K3 layer counts, expert counts, tensor names, manifest
hashes or environment variables — and it does not. Everything model-specific
reaches it as an `mzg2_profile` built by an adapter.

| File | Responsibility |
|---|---|
| `include/mzg2/format.h` | on-disk ABI: container structures, fixed sizes, symbol limits, error flags, the tile checksum |
| `include/mzg2/mzg2.h` | profile description, model build/validate, block validation, reference codec, store reader |
| `include/mzg2/decode_hip.h` | the GPU launch entry point |
| `src/profile.c` | profile consistency and derived tile geometry |
| `src/model.c` | static rANS model construction and the reader-side validation rules |
| `src/block.c` | host-side pre-decode validation, fail-closed with a message |
| `src/codec.c` | CPU reference encoder and decoder |
| `src/store.c` | sidecar headers, expert index, `(layer, expert)` extents, `O_DIRECT` spans |
| `src/decode_hip.hip` | wave32 tile decoder |

## The CPU reference is the normative implementation

`src/codec.c` reproduces the wave32 lane interleave on one thread: a round's
renormalization words are consumed in increasing lane order, which is exactly
what a 32-lane wave does with its ballot mask. One thread therefore decodes
what a wave decodes, bit for bit, and the GPU kernel is checked against it.

A v2 block does not carry its symbol width; the profile supplies it. See
`docs/extraction.md`.
