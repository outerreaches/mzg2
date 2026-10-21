# MZG2

MZG2 is a research project for **direct-to-destination GPU decompression of
independently streamed MoE expert weights**: compressed experts move from
storage straight into an admitted expert-cache slot on the GPU, with no CPU
decode step and no extra admission copy.

The reference result uses static rANS over independently bounded 16 KiB tiles,
reads compressed blocks from GPU-mapped `O_DIRECT` staging, and decodes
directly into the destination on AMD `gfx1151` (Ryzen AI Max+ 395, ROCm). On
the Kimi K3 research profile it reduced routed-expert storage traffic by
**13.0674%** and improved measured end-to-end fixtures by roughly **10–13%**,
with exact generated tokens and values in the recorded gates.

## Status

**Public draft, first published 2026-10-15** (tag `v0.1.0-draft`). This
repository is the **specification and design home for MZG2**: the format
specification (draft v0.1), architecture, provenance, security notes, and
roadmaps are public here. The specification stays pre-normative until the golden
vectors and CPU reference codec land with the extraction (see
[`docs/roadmap.md`](docs/roadmap.md)).

The qualified reference implementation — the K3 codec, transcode tools, and
their tests — ships today as part of the
**[Moonshine](https://github.com/outerreaches/moonshine)** project. The
model-independent core (format/codec API, CPU reference codec, golden vectors,
HIP decoder) will be extracted into this repository; see
[`docs/roadmap.md`](docs/roadmap.md) for the extraction plan and gates.

MZG2 is not yet a universal GPU codec. Initial support is limited to the Kimi
K3 research profile on ROCm wave32 hardware. A second model adapter — GLM 5.3
first when official weights land, DeepSeek V4 Pro as the alternative — remains
the gate that tests whether the core boundary is genuinely reusable before any
general release.

## Contents

- [`docs/format.md`](docs/format.md) — **MZG2 format specification, draft
  v0.1** (K3 profile, format version 2): container layout, model tables, tile
  decode, integrity, versioning.
- [`docs/architecture.md`](docs/architecture.md) — objective, reference
  pipeline, core/adapter split, performance gate.
- [`docs/provenance.md`](docs/provenance.md) — source lineage, prior art, and
  the code-lineage audit summary.
- [`docs/security.md`](docs/security.md) — threat model, checksum properties,
  error semantics, and planned hardening.
- [`docs/roadmap.md`](docs/roadmap.md) — phases from the frozen K3 reference
  to a public codec release.
- [`docs/optimization-roadmap.md`](docs/optimization-roadmap.md) — the
  measured optimization funnel and closed ideas.
- `core/`, `adapters/`, `bench/`, `tests/` — extraction targets (see their
  READMEs); the working analysis scripts live in `bench/`.

## Correct claims

The K3 transform is **dequantized-value-preserving** and exact against its
zero-gauge-canonical representation. It is **not** byte-exact to source MXFP4
negative zero. The rANS algorithm and GPU interleaving patterns have
substantial prior art (see `docs/provenance.md`); the MZG2 contribution is the
SSD-to-GPU-cache systems composition, its fail-closed format design, and its
end-to-end evidence — not a new entropy coder.

## Data and weights

No model weights or derived expert-store blocks belong in this repository.
Users must obtain model artifacts under their original licenses and derive any
local store themselves. The format specifies a source-manifest binding so a
store states which source revision it was derived from.

## License

Project-authored source and documentation are released under MIT (`LICENSE`).
Extraction may retain additional MIT notices for inherited Moonshine, DS4, or
GGML lineage. See [`docs/provenance.md`](docs/provenance.md).

---

Maintained by Alex Mazurovsky / [Outer Reaches](https://outerreaches.si).
