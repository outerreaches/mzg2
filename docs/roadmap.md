# Roadmap

Status of each phase is stated inline; the K3 reference remains the
regression profile throughout. Optimization details, kill gates, and closed
ideas live in [`optimization-roadmap.md`](optimization-roadmap.md).

For the immediate sequence, use [the release checklist](releasing.md): CPU
hardening, CI, documentation and a compile-only HIP runner can be prepared
while the GPU is occupied. GPU parity and performance runs wait for a
dedicated window. The next release is K3-scoped; broader reuse follows later.

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
- **Broader hardening.** CPU ASan/UBSan, four bounded synthetic fuzz targets,
  API-boundary regressions and GCC/Clang CI now exist. A short fuzz smoke does
  not establish hostile-input safety; longer campaigns remain useful.
- **Hardware execution.** The launch API and prepared parity runner reject
  non-wave32 devices. The runner compiles for `gfx1151`; execution and the
  device checks remain unqualified until a GPU window. A wave64 implementation
  is later work.
- **Standalone writer, optional.** The writer remains in Moonshine; the
  [pinned public route](store-building.md) now documents building and checking
  a store. A separate local writer is not required for the scoped release.

## Phase 3 — screen MZG2 v3 — planned

CPU-only simulator for compact metadata, compact models, and packed initial
states. Stop if exact combined gain is below 0.25 raw-byte percentage points.
GPU gates only after a positive screen; a full derived-store build requires
explicit approval.

The extraction adds one candidate to the v3 list: a block does not describe
its own symbol width, so a decoder cannot work from the bitstream alone.
Moving plane widths into block metadata would close that.

The 15-symbol-per-model limit is another explicit design constraint. A
full-byte FP8 or full-16-code MXFP4 representation does not fit the current
tables merely because a profile can specify 8-bit or 4-bit symbol width.
Evaluate that requirement independently of the metadata-size screen.

## Phase 4 — second adapter — planned, and the gate that matters

Evaluate GLM 5.3 first; DeepSeek V4 Pro is the alternative. Reuse the
screening funnel and the generic interface, not K3's bitstream parameters. If
the core interface needs substantial redesign, keep MZG2 K3-specific rather
than forcing a false abstraction.

The core boundary now exists in code, but the v2 ABI still reflects K3's
restricted alphabet. First determine whether the second model fits it; if
not, specify a representation or a versioned extension while preserving v2.
Existing GLM/MiMo codec experiments are candidates to compare, not evidence
that this unchanged core already supports those families.

## Phase 5 — source publication done; qualified code release pending

The public remote (`https://github.com/outerreaches/mzg2`) was created after
the provenance, clean-build, and security gates passed, and the model-license
and reproducibility documentation shipped with it. The codec extraction
followed on 2026-10-20. Code, specification, and evidence are published;
weights and derived stores are not, and will not be.

The existing release/tag is `v0.1.0-draft`, containing the documents. The
0.2.0 extraction is on `main` without a matching code release. Closing the
[candidate checklist](releasing.md) is the next publication milestone; source
availability is not a completed GPU or implementation-hardening gate.

The second-adapter gate (Phase 4) remains open. Rather than hold publication
on it, the published documents state the K3-only scope explicitly — README
and [`format.md`](format.md) both — so nothing published claims general
support.
