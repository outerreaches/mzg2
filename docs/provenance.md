# Provenance and prior art

## Source lineage

The qualified K3 implementation was developed in Moonshine, an MIT-licensed project extracted from the DS4 tree with retained DS4 and GGML notices. The exact Moonshine extraction commit for MZG2 has not yet been recorded because the implementation is still uncommitted in the working tree. No source should be copied here until that history is made explicit.

## Design influence

MZG2 does not invent rANS or GPU ANS interleaving. Relevant work includes:

- DietGPU, a CUDA GPU ANS and numerical compression library;
- hipANS, a HIP port and optimization of DietGPU;
- hipCOMP/nvCOMP GPU compression libraries;
- Recoil and prior massively parallel ANS decoding work;
- ZipServ and related lossless model-weight compression systems.

The K3 design note explicitly describes its wave-coalesced renormalization as the “DietGPU pattern.” Extraction must determine whether this was design influence only or whether any implementation text has source lineage requiring notice.

## MZG2 contribution

The intended contribution is a measured systems composition: expert-local random access, MXFP4-aware modeling, mapped direct-I/O input, bounded GPU decode directly into admission slots, fail-closed publication, and full-engine evidence on a consumer AMD unified-memory system.
