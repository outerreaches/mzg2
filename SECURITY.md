# Security policy

MZG2 is published as a specification draft: the format specification is public, the codec
implementation is not yet released in this repository, and there are no supported versions.

**Trust model:** the decoder assumes trusted local model stores. Decoder
errors invalidate the entire destination buffer, including bytes written
before the error was reported, and the tile checksum detects accidental
corruption only — it is not cryptographic and is not an authenticity
mechanism. Do not decode stores from untrusted sources.

The full model — defenses, limits, and the planned pre-normative hardening
(fuzzing, malformed-input matrix, hostile-input boundary definition) — lives
in [`docs/security.md`](docs/security.md).

## Reporting

Use GitHub security advisories on this repository once public, or the contact
route listed on the repository profile. Please do not open public issues for
memory-safety reports before a fix is available.
