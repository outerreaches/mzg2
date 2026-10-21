# Changelog

## 0.2.0 — 2026-10-20 — the codec extraction

The repository was the specification home; it is now the implementation home
as well. Full record and evidence: [`docs/extraction.md`](docs/extraction.md).

### Added

- **Model-independent core** (`core/`): the format ABI, profile description,
  static rANS model construction and validation, host-side pre-decode
  validation, the store reader, and the HIP wave32 decoder. It holds no K3
  constant — no layer count, expert count, tensor name, manifest digest or
  environment variable.
- **CPU reference codec** (`core/src/codec.c`): portable C encoder and
  decoder, now the normative implementation. It reproduces the wave32 lane
  interleave on a single thread, so one thread decodes what a wave decodes,
  bit for bit, which makes it the GPU kernel's oracle.
- **Kimi K3 adapter** (`adapters/kimi-k3/`): geometry, the six MXFP4 planes,
  the zero-gauge transform, the pinned source-manifest binding, and the
  tensor-name suffixes a transcoder needs.
- **Golden vectors** (`tests/vectors/`): three committed blocks plus a
  manifest, covering the mixed coded/raw case, a uniform payload, and the
  entropy coder's degenerate identity case. Synthetic, from a seed.
- **Tests**: 316 model-free checks across five binaries, with one injected
  defect per validation rule — 45 block cases and 30 store cases — so every
  gate has been seen to fail.
- **Tools**: `mzg2_store_check` (decode an existing store, and with
  `--reencode` require `encode(decode(block)) == block`) and `mzg2_vectors`.
- **Build**: `Makefile` with `make`, `make test`, `make vectors`, `make hip`.
  The CPU targets need no GPU, no ROCm and no model weights.

### Changed

- **`docs/format.md` is normative for format version 2.** The two artifacts
  it waited on — a CPU reference codec and golden vectors — now exist. The
  document also states where symbol width comes from, and flags the two rules
  the published core enforces more strictly than the original reader.
- `docs/architecture.md` records the core/adapter split as code, with a file
  map.
- `docs/provenance.md` pins the extraction lineage file by file.
- `docs/security.md` notes that each defense now has an injected-defect test,
  and that the HIP decoder here is unrun.
- `docs/roadmap.md`: Phase 2 extraction done, with its remaining items named.
- `README.md`, `CONTRIBUTING.md` and the per-directory READMEs rewritten for
  a repository that contains code.

### Evidence

- 316 model-free checks pass on a clean `-Wall -Wextra -Wpedantic -Werror`
  build; the HIP decoder compiles for `gfx1151`.
- Against the qualified Kimi K3 derived store: 14 experts — layer 1 experts
  0–2 and 100–101, and experts 440–442 of layers 1, 46 and 92 — opened under
  the source-bound profile, decoded with no error flags, and **re-encoded
  byte-identically to the stored blocks**. Sampled reduction
  12.838%–13.111%, bracketing the recorded full-store 13.0674%.

### Known gaps

- **The HIP decoder has not been run in this repository.** It is a direct
  port, reviewed against the qualified kernel, but the parity gate against
  the CPU reference is still owed.
- No published store writer; building a K3 store still means the Moonshine
  transcoder.
- wave64 unspecified; no fuzzing; one model adapter.

## 0.1.1 — documentation pass, 2026-10-20

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
