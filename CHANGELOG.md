# Changelog

## Unreleased — documentation pass, 2026-10-21

- Status language corrected now that the repository is public: `README.md`,
  `SECURITY.md`, and roadmap Phase 5 no longer describe the repository as being
  prepared for release.
- Roadmap Phase 5 marked **done (2026-10-15)**, with Phase 4 (second adapter)
  explicitly still open and the K3-only scope stated in the published documents.
- Brand links point at `https://outerreaches.si`; the `outerreaches.space`
  hostname redirects there.

## 0.1.0-draft — 2026-10-15 (published)

Repository assembled for public release:

- **Format specification draft v0.1** (`docs/format.md`): container layout,
  probability models, tile decode (including the wave-coalesced
  renormalization and terminal invariants), checksum, integrity contract,
  versioning, and the K3 profile constants — written from the qualified
  implementation and marked pre-normative pending golden vectors and the CPU
  reference codec.
- Provenance rewritten with the frozen source identities, the DietGPU
  code-lineage audit outcome, prior-art citations, and the AI-assistance
  disclosure (`docs/provenance.md`).
- New security model document (`docs/security.md`); `SECURITY.md` policy
  updated to match.
- `README.md` repositioned as the specification-and-design home; the
  qualified K3 implementation remains published in the Moonshine project.
- Release metadata: `CITATION.cff`, expanded `CONTRIBUTING.md`, this
  changelog.
- `bench/` analysis scripts scrubbed for public use (no local paths).
- Roadmap updated to current phase statuses (`docs/roadmap.md`).

Note: no codec implementation is published in this repository yet; the
standalone core, CPU reference codec, and golden vectors land with the
extraction (see the roadmap).
