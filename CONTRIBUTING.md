# Contributing

This repository currently carries the MZG2 specification and design
documentation; the model-independent codec, CPU reference codec, and golden
vectors arrive with the extraction (see `docs/roadmap.md`). Format review and
documentation issues are welcome now — that is the point of publishing the
draft early.

Core rules, for documents and (future) code alike:

1. Do not add model weights or derived expert-store blocks. No exceptions.
2. Keep model-specific tensor logic in an adapter; the core never learns K3
   layer counts, expert counts, tensor names, or environment variables.
3. Every behavioral claim needs a model-free test, a golden vector, or a
   measured run attached; projections and measurements are labeled
   distinctly.
4. Treat destination data as invalid after any decode error, and keep that
   contract tested.
5. Do not promote a codec change from microbenchmarks alone; full-engine A/B
   remains the performance gate.
6. Preserve source lineage and design citations (`docs/provenance.md`);
   changes cite the prior art they build on.
7. Keep changes small, evidence-backed, and explicit about tested hardware
   and model identity.
8. Never commit local absolute paths, host names, or service details.

The AI-assistance disclosure in `docs/provenance.md` describes how this work
is produced; contributions should meet the same evidence bar.
