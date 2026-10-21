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
  untrusted sources.** A normative bitstream specification defines compatible
  bytes; it does not establish implementation hardening. Bounded synthetic
  fuzzing is available; the extracted GPU execution gate remains open.

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

The initial extraction had 45 block assertions and 30 store rejection cases.
The shared block matrix now injects 43 defects and checks exact valid recovery
after each. API regressions add count/extent overflow, invalid profile/width,
unaligned input and overlapping decode-buffer checks. See
[the test inventory](../tests/README.md) for current coverage. Passing these
tests does not prove every possible malformed input is handled safely.

**The HIP decoder in this repository has not been executed** (see
`docs/extraction.md`). Its in-decode checks are a line-by-line port of the
qualified kernel's, but the claims above are evidenced for the CPU reference
codec only until the parity run happens.

## What the tile checksum does and does not do

The checksum is a non-cryptographic 32-bit folded XOR over output dwords
(`docs/format.md` §8.4).

- **Detects:** many accidental changes to tile payloads and decode output,
  alongside the bounds and terminal-state checks. The folded XOR is linear:
  cancelling changes can collide. No uniform random-error detection rate or
  exhaustive corruption coverage is claimed.
- **Does not:** authenticate data, resist deliberate tampering (an adversary
  can compute valid checksums), or cover metadata regions (headers,
  descriptors, and model tables are structurally validated but not
  checksum-protected). The v2 design keeps the checksum non-cryptographic by
  choice for decode speed. Stronger metadata protection requires a separately
  specified format or container change; the current research release retains
  the trusted-local-store boundary.

## Error codes

Per-block error flags (any nonzero = fail): `BOUNDS` (geometry/validation),
`STATE` (rANS state violations), `CHECKSUM` (tile checksum mismatch),
`TERMINAL` (final invariant violations). See `docs/format.md` §9 for the
exact contract.

## Remaining implementation qualification

- The CPU parser, model tables, round trips and synthetic store parser have
  [bounded fuzz targets](../tests/fuzz/README.md) with ASan/UBSan. Extend their
  corpus and duration as needed; replay malformed blocks through the GPU gate
  during an available window.
- Extend the existing malformed-input matrix with sanitizer coverage and
  retained fuzz regressions. Check destination guards and recovery after a
  failed decode; do not interpret partially written output as success.
- Run the extracted HIP decoder against CPU results and injected defects
  during a dedicated GPU window. Compilation alone does not close this gate.
- Keep the destination-invalidation contract in the public API and caller
  examples. It applies to the complete destination, not only the failed tile.

## Reporting

Security-relevant findings: follow [SECURITY.md](../SECURITY.md). Please do not
open public issues for memory-safety reports before a fix is available.
