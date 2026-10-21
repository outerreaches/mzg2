# Security model

MZG2 is a local, on-disk format decoder for model weights that users derive
themselves. This document states what the format and its decoder defend
against, what they do not, and which guarantees are contracts.

## Trust model

- **Trusted input:** store files produced by the project's transcode tools (or
  a compatible writer) on hardware the operator controls. Storage is trusted
  to be non-malicious; accidental corruption (bit rot, truncation, torn
  writes, interleaving) is the primary fault class the format defends against.
- **Not defended against:** adversarial stores. The structural validation
  below bounds the blast radius of malformed files, but the CPU parser and the
  HIP decode kernels are **not** hardened against crafted inputs, and the
  checksum is not an authentication mechanism. **Do not decode stores from
  untrusted sources.** Hardening and fuzzing are planned ahead of normative
  status (see below).

## Active defenses (by design)

- **Fail-closed store selection:** every sidecar is validated before use; the
  K3 profile pins the `source_manifest_sha256` binding and rejects mismatched
  stores; a directory store must be complete and duplicate-free; missing or
  invalid stores fail the open rather than falling back to unvalidated data
  (the raw SafeTensors path is a separate, explicit configuration).
- **Pre-decode structural validation:** every invariant in the format
  specification (`docs/format.md` §§2–8) is checked before any decode work:
  magic/versions/flags, all offsets and lengths range-checked with
  overflow-safe arithmetic, region ordering, alignments, reserved fields,
  descriptor geometry, and full model-table validation (frequency/cumulative
  bookkeeping, decode-table consistency, alphabet constraints).
- **In-decode checks at wave granularity:** initial rANS states at or above
  the lower bound, symbol validity, bounded word-stream consumption, strict
  terminal invariants (final state must land exactly on the lower bound), and
  a per-tile checksum.
- **Invalidation contract:** any nonzero error flag means the destination is
  invalid; there is no partial-success mode and callers must not publish
  flagged output. Abort paths drain outstanding I/O and GPU work before memory
  reuse in the surrounding engine.
- **Small fixed ABI:** structure sizes are asserted at compile time; reserved
  fields and unknown flags/kinds are rejection conditions, not warnings.

Since 2026-10-20 each of these defenses has a test that injects one defect
into an otherwise valid block or store and requires the rejection: 45 cases
over the block rules and integrity checks, 30 over the store rules
(`tests/test_malformed.c`, `tests/test_store.c`). A defense that has never
been observed to fire is not counted as one.

**The HIP decoder in this repository has not been executed** (see
`docs/extraction.md`). Its in-decode checks are a line-by-line port of the
qualified kernel's, but the claims above are evidenced for the CPU reference
codec only until the parity run happens.

## What the tile checksum does and does not do

The checksum is a non-cryptographic 32-bit folded XOR over output dwords
(`docs/format.md` §8.4).

- **Detects:** accidental corruption affecting tile payloads and decode
  output, including truncation and partial writes, with the coverage of a
  well-distributed 32-bit redundancy per 16 KiB tile.
- **Does not:** authenticate data, resist deliberate tampering (an adversary
  can compute valid checksums), or cover metadata regions (headers,
  descriptors, and model tables are structurally validated but not
  checksum-protected). The v2 design keeps the checksum non-cryptographic by
  choice for decode speed; whether metadata gains stronger protection is an
  open release-gate decision.

## Error codes

Per-block error flags (any nonzero = fail): `BOUNDS` (geometry/validation),
`STATE` (rANS state violations), `CHECKSUM` (tile checksum mismatch),
`TERMINAL` (final invariant violations). See `docs/format.md` §9 for the
exact contract.

## Planned hardening (before normative status)

- Fuzz the CPU parser and model-table validation; replay malformed blocks
  through the GPU gate where practical.
- A malformed-input test matrix covering truncation, overflow, overlapping
  descriptors, invalid models, invalid states, bad terminal cursors, raw
  escapes, and output aliasing.
- An explicit statement in the specification that destination contents are
  invalid after any decode error (already in §9) and tests asserting it.

## Reporting

Security-relevant findings: use GitHub security advisories on this repository
once public, or the contact route listed on the repository profile. Please do
not open public issues for memory-safety reports before a fix is available.
