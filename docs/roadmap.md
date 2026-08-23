# Roadmap

## Phase 0 — freeze the K3 reference

- Review and commit Moonshine MZG2.
- Record exact source and artifact identities.
- Rebuild and run direct clean-checkout gates.

## Phase 1 — extract

- Move generic format/codec code without K3 constants.
- Retain K3 behavior in an adapter.
- Add CPU reference code and synthetic golden vectors.

## Phase 2 — harden

- Finish the format specification.
- Add malformed-input tests and fuzzing.
- Define wave32/wave64 support and the checksum threat model.
- Publish repeatable mapped-input and concurrent-compute benchmarks.

## Phase 3 — second adapter

Evaluate GLM 5.3 first when official weights arrive. DeepSeek V4 Pro is the alternative. Screen each model from first principles rather than applying the K3 bitstream unchanged.

## Phase 4 — public release

Create a public remote only after provenance, clean-build, security, model-license, and second-adapter gates pass. Publish code and evidence, never weights or derived stores.
