# Contributing

This repository is private and pre-release. Keep changes small, evidence-backed, and explicit about tested hardware and model identity.

Core rules:

1. Do not add model weights or derived expert-store blocks.
2. Keep model-specific tensor logic in an adapter.
3. Add a model-free test for generic format or decoder behavior.
4. Treat destination data as invalid after any decode error.
5. Do not promote a codec from microbenchmarks alone; retain a full-engine A/B gate.
6. Preserve source lineage and design citations.
