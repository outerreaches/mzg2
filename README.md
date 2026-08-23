# MZG2

MZG2 is a research project for direct-to-destination GPU decompression of independently streamed MoE expert weights.

The reference result uses static rANS over independently bounded 16 KiB tiles, reads compressed blocks from GPU-mapped `O_DIRECT` staging, and decodes directly into an admitted expert-cache slot on AMD `gfx1151`. The original Kimi K3 integration reduced routed-expert storage traffic by 13.0674% and improved measured end-to-end fixtures by roughly 10–13%.

## Status

This repository is a private, docs-first extraction scaffold. It has no public remote and does not yet contain a released codec implementation. The qualified implementation remains in the Moonshine working tree until it is reviewed, committed, and extracted with provenance intact.

MZG2 is not yet a universal GPU codec. Initial support is limited to the Kimi K3 research profile on ROCm wave32 hardware. A second model adapter—planned for GLM 5.3 or DeepSeek V4 Pro—will test whether the core boundary is genuinely reusable.

## Intended layout

- `core/`: model-independent format and codec API.
- `adapters/kimi-k3/`: MXFP4 transform, tensor discovery, and store integration.
- `bench/`: mapped-input and concurrent-compute benchmarks.
- `tests/`: golden vectors, parser/corruption tests, and HIP qualification.
- `docs/`: architecture, draft format, provenance, security, and roadmap.

Current priorities and kill gates are in [`docs/optimization-roadmap.md`](docs/optimization-roadmap.md). The P0 ownership and scheduling design is in [`docs/p0-existing-format-plan.md`](docs/p0-existing-format-plan.md). The frozen K3 implementation boundary and Moonshine commit plan are in [`docs/k3-source-inventory.md`](docs/k3-source-inventory.md).

## Correct claims

The K3 transform is **dequantized-value-preserving** and exact against its zero-gauge-canonical representation. It is not byte-exact to source MXFP4 negative zero. The rANS algorithm and GPU interleaving patterns have substantial prior art; the MZG2 contribution is the SSD-to-GPU-cache systems integration and its end-to-end evidence.

## Data and weights

No model weights or derived expert-store blocks belong in this repository. Users must obtain model artifacts under their original licenses and derive any local store themselves.

## License

Project-authored source and documentation are intended for release under MIT. Extraction may retain additional MIT notices for inherited Moonshine, DS4, or GGML lineage. See [`docs/provenance.md`](docs/provenance.md).
