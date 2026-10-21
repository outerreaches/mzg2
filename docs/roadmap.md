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

## Phase 2 — extract and harden — in progress

- **Format specification:** draft v0.1 published in this repository
  ([`format.md`](format.md)); normative status pending reference vectors.
- **Model-independent core:** to be extracted without K3 constants; K3
  behavior retained in an adapter.
- **CPU reference codec + golden vectors:** next major artifact; required for
  normative status.
- Malformed-input tests and fuzzing; explicit wave32/wave64 and
  unified/discrete-memory backend statements.

## Phase 3 — screen MZG2 v3 — planned

CPU-only simulator for compact metadata, compact models, and packed initial
states. Stop if exact combined gain is below 0.25 raw-byte percentage points.
GPU gates only after a positive screen; a full derived-store build requires
explicit approval.

## Phase 4 — second adapter — planned

Evaluate GLM 5.3 first when official weights arrive; DeepSeek V4 Pro is the
alternative. Reuse the screening funnel and the generic interface, not K3's
bitstream parameters. If the core interface needs substantial redesign, keep
MZG2 K3-specific rather than forcing a false abstraction.

## Phase 5 — public release — done (2026-10-15)

The public remote (`https://github.com/outerreaches/mzg2`) was created after the
provenance, clean-build, and security gates passed, and the model-license and
reproducibility documentation shipped with it. Code, specification, and evidence are
published; weights and derived stores are not, and will not be.

The second-adapter gate (Phase 4) remains open. Rather than hold publication on it, the
published documents state the K3-only scope explicitly — README and
[`format.md`](format.md) both — so nothing published claims general support.
