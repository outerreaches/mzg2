# K3 source freeze and extraction inventory

Date: 2026-08-23

## Purpose

Preserve the qualified Moonshine MZG2 implementation before extracting or optimizing it. This document first recorded the broad uncommitted working tree, then its reviewed resolution, without copying implementation code into this repository.

The corresponding machine-readable capture identity is maintained in the private evidence repository at `manifests/k3-runtime-source-freeze-20260823.json`. The capture remains immutable; the current clean source identity is the commit sequence below.

## Clean-source resolution

Moonshine `main` is clean at `c773930` and contains the captured work in five local commits:

1. `c5b65c7` — portable prefix and route planning cores;
2. `c20dae7` — portable static-Q8 screening core;
3. `4dc9506` — MZG1/MZG2 codecs, builders, and tests;
4. `cdc1755` — engine/build/fixture/documentation integration;
5. `c773930` — fail-closed store/model validation and diagnostic-capture hardening.

The final hardening commit also records DietGPU as design prior art and Zstandard as a separate platform dependency. Builds and runtime tests remain deferred until the active workload ends.

## Runtime identity

The captured Moonshine service reported `experts=mzg2-experiment`. Its on-disk executable matched the active process executable:

- Moonshine base commit: `ce862cc0e0664bed2befe3f3b6f65f12b4e54f1f`
- Executable SHA-256: `0ac768b691241ef3ff092b4dabcfda7fc17628c71c6db51707e76dfc0856079f`
- Executable bytes: 1,598,472
- Service-ready time: `2026-08-23T00:57:46.801Z`

This does **not** prove that the current dirty source tree rebuilds the executable. The runtime-core `.cu`/`.inc` files were not newer than the executable, but `k3_engine.h` was edited after the build. The binary and source freeze are therefore preserved as separate identities.

## Implementation modules

### Codec/store core

| file | responsibility |
|---|---|
| `k3_mzg2.h` | K3 format ABI, model tables, store/span/launch API |
| `k3_mzg2.cu` | sidecar validation, store map, bounded wave32 decode, integrity reporting |

The decoder file includes only the MZG2 public header plus HIP/system headers. It is technically easy to copy, but its API and constants are still K3-specific.

### Derived-store builder

| file | responsibility |
|---|---|
| `tools/transcode_mzg2_layer.cu` | K3 tensor discovery, zero-gauge transform, model construction, encoding, CPU verification |
| `tools/transcode_mzg2_full.py` | resumable 92-layer orchestration, sidecar hashes, second-pass verification, atomic publication |

The layer tool depends on Moonshine's SafeTensors reader. The full builder invokes the layer tool and hard-codes the K3 source identity and geometry.

### Runtime integration

| file | responsibility | extraction concern |
|---|---|---|
| `k3_engine.cu` | store open, memory planning, mapped staging, span selection, direct-to-slot decode, integrity gate | heavily mixed with unrelated dirty work |
| `k3_engine_prefill.inc` | temporary output buffers and selected-prefill decode path | mixed file; current per-expert synchronization is P0 work |
| `k3_engine.h` | profile/stat field | edited after captured binary build |
| `k3_server.c`, `k3_chat_cli.c` | selected-store reporting | small isolated hunks |
| `tests/test_k3_engine_init.cu` | MZG2 allocation/profile accounting | direct bounded fixture |
| `Makefile` | objects and transcode tool | mixed with several unrelated untracked modules |

### Documentation

`README.md`, `docs/getting-started.md`, `docs/architecture.md`, and `CHANGELOG.md` contain the qualified store identity and results. They should follow the implementation commit rather than lead it.

### Qualification source

The standalone GPU benchmark and context screen remain in the private Moonshine experiment tree. Their hashes are recorded in the evidence manifest. Curated aggregate results already live in the experiments repository.

## Dirty-tree coupling

The MZG2 integration cannot safely be committed as one blind working-tree snapshot:

- `k3_engine.cu` has 32 changed hunks; only 14 mention MZG2 directly.
- `k3_engine_prefill.inc` has six changed hunks; two mention MZG2 directly.
- `Makefile` has 12 changed hunks; five mention MZG2 directly.
- The build also references untracked MZG1, prefix catalog, route-index, Q8 codec, and static-layout files.
- Engine/chat/server files contain other unfinished behavior changes.

Every MZG2 hunk therefore needs a surgical review or a precursor commit for its real dependency. Do not commit all dirty Moonshine work under an MZG2 message.

## Qualification still required

The source history is clean, but source qualification is intentionally incomplete while the accepted workload owns the GPU. After it ends:

1. rebuild from a detached clean worktree at `c773930`;
2. run portable/model-free gates and the warning-free full build;
3. run bounded MZG corruption/model-table fixtures;
4. run engine initialization and selected-prefill exactness;
5. rerun an accepted MZG2 engine fixture before treating `c773930` as a replacement runtime identity.

## Extraction boundary after Moonshine is clean

The standalone core must not know K3 layer count, expert count, tensor names, manifest hash, environment variables, or cache policy. The K3 adapter retains those responsibilities and must reproduce the accepted Moonshine behavior before any v3 format work begins.
