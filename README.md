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

**Public, and the codec ships here as of 2026-10-20** — the model-independent
core, the CPU reference codec, the store reader, golden vectors, the HIP
decoder and the Kimi K3 adapter, extracted from the qualified implementation.
The repository was first published on 2026-10-15 (tag `v0.1.0-draft`) as the
specification and design home; this is the extraction that release pointed at.

What that gives you: a portable C library that encodes and decodes the v2
bitstream with no GPU, no ROCm and no model weights, plus a HIP kernel for the
wave32 path. What it does not yet give you: a published store writer (building
a K3 store still means the
[Moonshine](https://github.com/outerreaches/moonshine) transcoder), a wave64
kernel, or a second model adapter.

Two limits worth stating up front:

- **The HIP decoder compiles but has not been run in this repository.** It is
  a direct port of the qualified kernel, reviewed against it, and the CPU
  reference is its oracle — but the parity run is still owed. See
  [`docs/extraction.md`](docs/extraction.md).
- **MZG2 is not a universal GPU codec.** One adapter exists. A second model
  family — GLM 5.3 first, DeepSeek V4 Pro as the alternative — is the gate
  that tests whether the core boundary is genuinely reusable. The
  specification and this README state the K3-only scope rather than holding
  publication on it.

## Build and test

No GPU, no ROCm and no model weights are needed for the CPU targets:

```sh
make            # library, tools and the test suite
make test       # 316 model-free checks across five binaries
make hip        # the HIP decoder (needs ROCm; GPU_ARCH=gfx1151 by default)
```

To check an existing store — it decodes blocks and, with `--reencode`,
requires `encode(decode(block)) == block`:

```sh
./tools/mzg2_store_check kimi-k3 /path/to/expert-store-mzg2 --experts 4 --reencode
```

## Contents

Specification and design:

- [`docs/format.md`](docs/format.md) — **the MZG2 format specification**
  (v2): container layout, model tables, tile decode, integrity, versioning.
- [`docs/architecture.md`](docs/architecture.md) — objective, reference
  pipeline, core/adapter split, performance gate.
- [`docs/extraction.md`](docs/extraction.md) — what the extraction moved, the
  findings it produced, and the evidence behind it.
- [`docs/provenance.md`](docs/provenance.md) — source lineage, prior art,
  code-lineage audit, AI-assistance disclosure.
- [`docs/security.md`](docs/security.md) — threat model, checksum properties,
  error semantics, planned hardening.
- [`docs/roadmap.md`](docs/roadmap.md) — phases and their status.
- [`docs/optimization-roadmap.md`](docs/optimization-roadmap.md) — the
  measured optimization funnel and closed ideas.

Code:

- [`core/`](core/README.md) — the model-independent format, reference codec,
  store reader and HIP decoder.
- [`adapters/kimi-k3/`](adapters/kimi-k3/README.md) — the qualified K3
  profile: geometry, MXFP4 plane layout, zero-gauge transform, source
  binding.
- [`tests/`](tests/README.md) — model-free tests and the golden vectors.
- [`tools/`](tools/README.md) — store checker and vector generator.
- [`bench/`](bench/README.md) — analysis scripts.

## Correct claims

The K3 transform is **dequantized-value-preserving** and exact against its
zero-gauge-canonical representation. It is **not** byte-exact to source MXFP4
negative zero. The rANS algorithm and GPU interleaving patterns have
substantial prior art (see `docs/provenance.md`); the MZG2 contribution is the
SSD-to-GPU-cache systems composition, its fail-closed format design, and its
end-to-end evidence — not a new entropy coder.

Measured numbers in this repository come from a single qualified
configuration on one machine. They are engineering fixtures, not benchmarks.

## Data and weights

No model weights or derived expert-store blocks belong in this repository.
Users must obtain model artifacts under their original licenses and derive any
local store themselves. The format specifies a source-manifest binding so a
store states which source revision it was derived from. The golden vectors in
`tests/vectors/` are synthetic: they are generated from a seed by
`tests/vectors.h` and have no model behind them.

## License

Project-authored source and documentation are released under MIT (`LICENSE`).
Extraction may retain additional MIT notices for inherited Moonshine, DS4, or
GGML lineage. See [`docs/provenance.md`](docs/provenance.md).

---

Maintained by Alex Mazurovsky / [Outer Reaches](https://outerreaches.si).
