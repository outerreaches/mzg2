# Roadmap

Optimization is ordered by risk-adjusted end-to-end impact. See [`optimization-roadmap.md`](optimization-roadmap.md) for candidate-specific measurements, kill gates, and closed ideas.

## Phase 0 — freeze the K3 reference

- Review and commit Moonshine MZG2 before optimization or extraction.
- Record exact source and artifact identities.
- Rebuild and run direct clean-checkout gates.
- Preserve the accepted raw/MZG2 fixtures as immutable baselines.

## Phase 1 — remove existing pipeline cost

- Sweep compressed-path queue depth.
- Replace per-expert prefill synchronization with safe per-buffer events.
- Clear integrity flags once per layer or batch.
- Specialize packed/scale decoding and reduce model/LUT staging on the existing bitstream.

Each change remains independent and must pass exactness, corruption, concurrent-compute, and bounded engine gates.

## Phase 2 — extract and harden

- Move generic format/codec code without K3 constants.
- Retain K3 behavior in an adapter.
- Add CPU reference code and synthetic golden vectors.
- Finish the format specification.
- Add malformed-input tests and fuzzing.
- Define wave32/wave64 and UMA/discrete input backends.

## Phase 3 — screen MZG2 v3

Build a CPU-only simulator for compact metadata, compact models, and packed initial states. Stop if exact combined gain is below 0.25 raw-byte percentage points. Only then run mapped-input and concurrent-compute GPU gates. A full derived-store build requires explicit approval.

## Phase 4 — second adapter

Evaluate GLM 5.3 first when official weights arrive. DeepSeek V4 Pro is the alternative. Reuse the screening funnel and generic interface, not K3's bitstream parameters.

## Phase 5 — public release

Create a public remote only after provenance, clean-build, security, model-license, second-adapter, and reproducibility gates pass. Publish code and evidence, never weights or derived stores.
