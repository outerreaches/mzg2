# Security policy

MZG2 publishes a research codec and a normative version-2 format specification.
The CPU reference codec, store reader, synthetic vectors, K3 adapter and HIP
decoder are on `main`. The extracted HIP decoder has not yet passed its GPU
execution gate; no production-hardened or hostile-input-supported version is
claimed. The existing `v0.1.0-draft` release contains the earlier documents.

**Trust model:** the decoder assumes trusted local model stores. Decoder
errors invalidate the entire destination buffer, including bytes written
before the error was reported, and the tile checksum detects accidental
corruption only — it is not cryptographic and is not an authenticity
mechanism. Do not decode stores from untrusted sources.

The full model — defenses, limits, and the remaining implementation hardening
(fuzzing, malformed-input matrix, hostile-input boundary definition) — lives
in [`docs/security.md`](docs/security.md).

## Reporting

Use the repository's private vulnerability reporting route if available, or
the contact route listed on the maintainer's profile. Please do not open
public issues for memory-safety reports before a fix is available.
