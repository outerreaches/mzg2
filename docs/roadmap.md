# Roadmap

Status of each phase is stated inline; the K3 reference remains the
regression profile throughout. Optimization details, kill gates, and closed
ideas live in [`optimization-roadmap.md`](optimization-roadmap.md).

## Phase 0 — freeze the K3 reference — done

- Moonshine MZG2 changes reviewed and committed (`c5b65c7` … `c773930`).
- Source and runtime identities recorded (see `provenance.md`).
- Clean-rebuild qualification was explicitly deferred while the workload
  owned the GPU; the raw/MZG2 fixtures remain the accepted baselines.

## Phase 1 — remove existing pipeline cost — screened in Moonshine

The P0 queue was screened 2026-08-24 against the promoted engine and closed
without retained source changes (details and verdicts in
[`optimization-roadmap.md`](optimization-roadmap.md)). Remaining
existing-format levers (kernel specialization, model staging) continue in
Moonshine under its own gates.

## Phase 2 — extract and harden — extraction done, 2026-10-20

Landed (see [`extraction.md`](extraction.md) for the record and the
evidence):

- **Model-independent core** with no K3 constants: the format ABI, profile
  description, model construction and validation, pre-decode validation, the
  store reader, and the HIP decoder.
- **CPU reference codec**, portable C, which reproduces the wave32 lane
  interleave on one thread and is the GPU kernel's oracle.
- **Kimi K3 adapter** holding the geometry, plane layout, zero-gauge
  transform and source binding.
- **Golden vectors** (`tests/vectors/`) plus 316 model-free checks, including
  one injected defect per validation rule.
- **Format specification promoted to normative for version 2**
  ([`format.md`](format.md)).
- Byte-level evidence against the qualified store: 14 experts across three
  layers decode and re-encode identically.

Still open in this phase:

- **Run the HIP decoder.** It compiles for `gfx1151` but was not executed
  during the extraction. The gate is full-block agreement with the CPU
  reference on the golden vectors and on real store blocks, with the error
  flags exercised by the same injected defects the CPU tests use.
- **Fuzzing** the validation surface, which is currently covered by
  hand-written cases.
- **An explicit wave64 statement**, and a kernel if one is wanted: the v2
  renormalization order is defined by 32 lanes.
- **A published store writer.** Building a K3 store still means the Moonshine
  transcoder.

## Phase 3 — screen MZG2 v3 — planned

CPU-only simulator for compact metadata, compact models, and packed initial
states. Stop if exact combined gain is below 0.25 raw-byte percentage points.
GPU gates only after a positive screen; a full derived-store build requires
explicit approval.

The extraction adds one candidate to the v3 list: a block does not describe
its own symbol width, so a decoder cannot work from the bitstream alone.
Moving plane widths into block metadata would close that.

## Phase 4 — second adapter — planned, and the gate that matters

Evaluate GLM 5.3 first; DeepSeek V4 Pro is the alternative. Reuse the
screening funnel and the generic interface, not K3's bitstream parameters. If
the core interface needs substantial redesign, keep MZG2 K3-specific rather
than forcing a false abstraction.

The core boundary now exists in code, which makes this testable rather than
hypothetical: a second adapter should need a profile, a plane layout and a
value transform, and nothing in `core/`.

## Phase 5 — public release — done (2026-10-15 documents, 2026-10-20 codec)

The public remote (`https://github.com/outerreaches/mzg2`) was created after
the provenance, clean-build, and security gates passed, and the model-license
and reproducibility documentation shipped with it. The codec extraction
followed on 2026-10-20. Code, specification, and evidence are published;
weights and derived stores are not, and will not be.

The second-adapter gate (Phase 4) remains open. Rather than hold publication
on it, the published documents state the K3-only scope explicitly — README
and [`format.md`](format.md) both — so nothing published claims general
support.
