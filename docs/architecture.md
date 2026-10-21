# Architecture

## Objective

Reduce physical expert traffic in storage-bound MoE inference without replacing it with CPU work, redundant DRAM traffic, synchronization, or GPU contention.

## Reference pipeline

```text
versioned expert block on storage
  -> direct asynchronous read
  -> GPU-visible mapped input
  -> bounded tile-parallel decoder
  -> caller-provided device destination
  -> consumer kernel
  -> integrity barrier
  -> publish destination/cache metadata
```

## Separation

The generic core owns block parsing, static entropy models, bounded tiles, raw escape, decode launch, and error reporting. Adapters own model identity, tensor layout, value transforms, expert geometry, store publication, and runtime admission.

The initial K3 adapter uses MXFP4 packed-nibble and scale-byte models. It canonicalizes E2M1 negative zero, so its contract is dequantized-value-preserving rather than byte-exact.

Since the 2026-10-20 extraction this is code, not intent:

| Concern | Where |
|---|---|
| container ABI, entropy models, tiles, validation, reference codec, store reader, HIP decoder | `core/` |
| magic, geometry, plane layout, symbol widths, value alphabet, value transform, source binding | `adapters/kimi-k3/`, via an `mzg2_profile` |
| store construction | published in Moonshine; not yet a tool in this repository |
| cache admission, I/O scheduling, staging | the consuming engine |

Within v2's limits, an adapter is a profile plus a plane layout plus a value
transform. The core has no K3 geometry or identity constants, and
`tests/fixture.h` exercises a synthetic profile. It still implements a
15-symbol, at-most-two-model ABI; full-byte FP8 is not supported by merely
changing a profile. A second real model family remains the reuse gate.

The CPU reference codec is the normative implementation and the GPU kernel's
oracle: it reproduces the wave32 lane interleave on one thread, so a single
thread decodes what a wave decodes, bit for bit.

## Performance gate

A candidate must:

- decode faster than the measured compressed storage demand with contention headroom;
- add no net admission copy relative to the selected raw path;
- avoid blocking storage completion processing;
- preserve the adapter's numerical contract;
- beat raw or simpler storage in a complete engine A/B.

The rejected CPU-Zstd MZG1 path is the standing counterexample to promotion from compression ratio or isolated throughput alone.
