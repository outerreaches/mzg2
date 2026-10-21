# Provenance and prior art

## Source lineage

The qualified K3 implementation was developed in
**[Moonshine](https://github.com/outerreaches/moonshine)**, an MIT-licensed
engine that was extracted from the DS4 tree with retained DS4 and GGML
notices. The original engine integration and store-building tools live there
(`k3_mzg2.h`, `k3_mzg2.cu`, `tools/transcode_mzg2_*`,
`tools/build_mzg2_bundle.py`, `tests/test_build_mzg2_bundle.py`). This repository
now carries the extracted codec as well as its format specification.

Qualification identities (August 2026):

- Moonshine base: `ce862cc0e0664bed2befe3f3b6f65f12b4e54f1f`.
- Clean-source resolution, five reviewed commits partitioning the MZG2 work:
  `c5b65c7` (portable prefix/route-planning cores), `c20dae7` (static-Q8
  screening core), `4dc9506` (MZG codecs, builders, tests), `cdc1755`
  (engine/build/fixture integration), `c773930` (fail-closed store/model
  validation and diagnostic-capture hardening).
- Historical runtime identity: executable SHA-256
  `0ac768b691241ef3ff092b4dabcfda7fc17628c71c6db51707e76dfc0856079f`,
  service ready `2026-08-23T00:57:46.801Z`. Runtime and source identities
  were captured separately (one engine header changed after the binary was
  built); an exact clean rebuild was an explicitly deferred qualification
  step, not a hidden one.

## Extraction lineage (2026-10-20)

The code in `core/` and `adapters/kimi-k3/` was extracted from the Moonshine
files below, at the clean-source commits recorded above. It is a
port and adaptation against the same format: the constants became profile
fields, the C++/HIP store reader became portable C, and the
transcoder's scalar encoder and verifier became the published reference
codec. No third-party source was introduced.

| Extracted into | From |
|---|---|
| `core/include/mzg2/format.h` | `k3_mzg2.h` (container ABI, model layout, error flags) |
| `core/src/block.c`, `core/src/store.c` | `k3_mzg2.cu` (host validation, store map, spans) |
| `core/src/decode_hip.hip` | `k3_mzg2.cu` (`decode_tiles_kernel`) |
| `core/src/codec.c`, `core/src/model.c` | `tools/transcode_mzg2_layer.cu` (`build_model`, `encode_tile`, `encode_block`, `decode_block`) |
| `adapters/kimi-k3/` | constants and the zero-gauge transform from both files |

The frequency-quantization arithmetic and its tie-breaks were ported
unchanged, because the resulting table bytes are part of a stored block.
`docs/extraction.md` records the two places where the published core is
stricter than the original reader, the implicit behaviours that became
explicit profile fields, and the byte-level evidence that the extracted codec
reproduces the qualified store.

No model weights, derived store blocks, logs, prompts or host paths were
moved. The golden vectors are synthetic.

## Code-lineage audit

- The entropy coder is **standard static rANS** with a wave-coalesced
  renormalization structure informed by Meta's
  [DietGPU](https://github.com/facebookresearch/dietgpu) GPU ANS design.
  The implementation is independent HIP/wave32 code; **no DietGPU source is
  vendored or copied** (recorded in Moonshine's provenance documentation).
  DietGPU is design prior art, not a runtime dependency.
- Zstandard was a separate platform dependency of the earlier MZG1 research
  path and offline screens; MZG2 does not use it.
- No other GPU-codec source was consulted beyond the published prior art
  listed below.

## Prior art

MZG2 does not invent rANS, nor the first GPU ANS codec. Relevant work
includes:

- **DietGPU** — GPU ANS and numerical compression library (Meta).
- **hipANS** — HIP port/optimization of the DietGPU codec.
- **hipCOMP / nvCOMP** — GPU compression libraries.
- **Recoil** and prior massively parallel ANS decoding work.
- **ZipServ** and related lossless model-weight compression systems.
- Standard static rANS (Duda; the widely used range-asymmetric numeral
  systems formulation).

## MZG2 contribution

The contribution claimed is a measured systems composition: expert-local
random access over independently bounded tiles, MXFP4-aware order-0 modeling
with canonicalized gauge, GPU-mapped `O_DIRECT` input, a bounded GPU decoder
that writes directly into the admission destination, a fail-closed format
with strict terminal invariants, and full-engine end-to-end evidence on a
consumer AMD unified-memory system.

## AI assistance disclosure

This project and the Moonshine engine it documents were produced with strong
assistance from AI coding and review systems. The human maintainer selected
the architecture, directed experiments, reviewed changes, operated the
hardware, and made correctness and release decisions.
