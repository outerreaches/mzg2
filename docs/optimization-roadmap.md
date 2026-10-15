# Optimization roadmap

This roadmap ranks work by risk-adjusted end-to-end impact. Estimates are screening targets, not accepted results. The Kimi K3 reference remains the regression profile.

## P0 — improve the existing pipeline without changing the bitstream

P0 was screened 2026-08-24 against the promoted MZG2 engine and closed without retained source changes: QD2 kept (QD1/4/8 rejected), layer-scoped integrity clearing and event-driven prefill reuse reverted on gate misses. The full screening design lives in the private experiments repository.

1. **Compressed-path queue-depth sweep:** test QD 1/2/4/8 against one route trace. Keep a new QD only for >=2% median pipeline improvement without tail or thermal regression.
2. **Event-driven prefill buffer reuse:** replace per-expert global stream synchronization with per-buffer events. Require safe input/output reuse, fail-closed integrity, exact output, and >=5% routed-prefill improvement.
3. **One integrity-flag clear per layer/batch:** remove per-expert `hipMemsetAsync`. Require no stale-bit failures and >=1% layer gain.

## P1 — decoder headroom on the existing format

1. Specialize packed and scale kernels and unroll their fixed output-dword loops.
2. Stage only the model used by a workgroup.
3. Pack `(frequency, cumulative)` into one 32-bit record.
4. Compare the full symbol LUT with an exact coarse LUT plus bounded correction.
5. Sweep 128/256/512-thread workgroups and native wave32 prefix primitives.
6. Probe direct-I/O-compatible allocation and cache policies.
7. Make wave-size and UMA/discrete input backends explicit and fail closed when unsupported.

Require exact output, complete corruption behavior, and at least 5% concurrent-compute decoder improvement for substantive kernel changes.

## P2 — CPU-only MZG2 v3 format screen

Combine, but measure separately:

- compact terminal-offset/checksum/raw-bitset metadata, expected +0.14–0.15 reduction points;
- compact palette/frequency model serialization, up to +0.047 points;
- bit-packed initial rANS states, estimated +0.11 points.

Stop if exact combined gain is below 0.25 points. The rough target is 13.35–13.38% K3 reduction versus 13.0674% today. This is only about 0.35% fewer physical bytes, so no full-store transcode follows without a positive GPU and layer-local gate.

Separately sweep 18/20/21/24 KiB tiles. Require >=0.08 points real gain and >=25 GiB/s under representative concurrent expert compute. Screen 16-state half-wave tiles and mixed rANS precision independently.

## P3 — deeper scheduling

1. Decode plane ranges in consumer order (`w1`, `w3`, `w2`); require >=5% combined decode+GEMV improvement.
2. Test a separate decode stream only with explicit active-reader and decode-complete events; require 3–5% full-layer improvement.
3. Batch only CQ completions already available. Never wait to form a larger batch.

## P4 — high-risk research

- Fuse decode with one MXFP4 GEMV while also writing the canonical cache slot. Require >10% combined gain on one tensor before expanding.
- Screen a non-materializing prefill path across realistic selected-token counts. Require >15% routed-expert gain.
- Add a GPU or tile-parallel CPU encoder after the format stabilizes.

## Closed for the K3 profile

Do not reopen without materially different data: scale-conditioned packed context, previous/pair context, tile-local or per-plane models, scale delta/RLE, additional MXFP4 gauge transforms, sign/magnitude or bitplane factorizations, tuple coding, simple 32/64 KiB tiles, CPU admission work, or universally compressed cache slots.

Universal compressed residency now has direct engine evidence: fixed-stride C34
gained 2.326 hit points over decoded C30 but regressed prompt wall 2.10% and
decode wall 4.75%. The exact prototype was reverted.

## Promotion funnel

Analytical/CPU screen -> exactness and corruption -> mapped-input benchmark -> concurrent compute -> layer-local A/B -> bounded full engine -> explicit approval before a full derived-store build.
