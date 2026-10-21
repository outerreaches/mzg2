# Next research release

The codec extraction is public on `main` (`4fe3ba2`). The existing
`v0.1.0-draft` tag and GitHub release describe the earlier documents. The
0.2.0 changelog entry records an untagged code milestone; it is not evidence
that the extracted GPU implementation passed qualification.

The next release stays scoped to the Kimi K3 research profile on `gfx1151`,
with a portable CPU reference. Format v2 being normative means its bytes and
contracts are specified. It does not imply hostile-input hardening, a stable
library API, GPU execution coverage, or multi-model support.

## Work that needs no GPU window

- [x] Publish the core, K3 adapter, CPU reference and synthetic golden vectors.
- [x] Record the extraction's 316 passing model-free checks and sampled
  byte-identical re-encoding of 14 qualified-store experts.
- [x] Reconcile documentation, security policy and development-version metadata.
- [ ] Add clean GCC/Clang CI and address/undefined-behaviour sanitizer gates.
- [ ] Add bounded, synthetic-only CPU fuzzing for blocks, model tables and
  store parsing; retain any discovered regression as a deterministic test.
- [ ] Prepare a HIP parity executable covering golden vectors, a bounded
  source-bound store sample, injected defects, guards and recovery. Compile
  it without executing it while another workload owns the GPU.
- [ ] Document a pinned public store-building route. A separate writer in
  this repository is optional for this release if the Moonshine route is
  reproducible and its dependencies and resource costs are explicit.

Use modest CPU parallelism and bounded runs on a serving host. Synthetic
tests need neither modelstore I/O nor model weights. Building a full derived
store, reading the full corpus or running inference is not part of this work.

## Dedicated GPU window

- [ ] From the exact candidate, run full-block HIP/CPU parity on all synthetic
  golden vectors and selected real K3 store blocks under the pinned profile.
- [ ] Exercise host rejection and device STATE/CHECKSUM/TERMINAL failures;
  preserve output guards and recover with a valid decode after each defect.
- [ ] Record GPU ISA, wave size, compiler/ROCm versions, candidate commit,
  fixture identities and results. Unsupported targets remain unqualified.
- [ ] Measure the extracted mapped-input path and representative compute
  contention before publishing performance numbers for this implementation.

Historical Moonshine results must remain labelled as historical engine
results. They do not automatically qualify this repository's kernel. A new
runtime integration or a new claim of inference speedup requires full-engine
A/B with numerical parity; a decode microbenchmark alone cannot establish it.

## Package the tested candidate

- [ ] Complete a clean-checkout run of documented CPU gates at the final head.
- [ ] Attach a concise reproducibility report with commands and gate status,
  separating freshly measured results from inherited evidence.
- [ ] Review source lineage/notices, model-license references, relative links,
  secret/path scans and the tracked artifact inventory. Synthetic vectors are
  allowed; weights, real store blocks and private traces are not.
- [ ] Reconcile README, changelog and citation metadata with the chosen version.
- [ ] Create a new tag and matching research release at the reviewed commit;
  preserve `v0.1.0-draft`. Record signing/verification status and remaining
  limitations. Publishing commits to `main` does not close these gates.

## Later work

Second-family reuse, wave64, stronger hostile-input protection and v3 format
experiments are separate milestones. v2 allows only 15 symbols per entropy
model, even for 8-bit planes; a full-byte FP8 adapter requires a representation
or format decision before implementation. Do not present it as a profile-only
port. The v3 metadata-size screen retains its 0.25 raw-byte percentage-point
gate; capacity and self-description are separate design motivations.

Model-weight bundle publication is outside this codec release. It has its own
rights, distribution and clean-download qualification requirements.
