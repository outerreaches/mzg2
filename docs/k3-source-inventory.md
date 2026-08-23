# K3 source freeze and extraction inventory

Date: 2026-08-23

## Purpose

Preserve the qualified Moonshine MZG2 implementation before extracting or optimizing it. The source of truth is still a broad, uncommitted Moonshine working tree, so this document records boundaries and commit order without copying implementation code into this repository.

The corresponding machine-readable identity is maintained in the private evidence repository at `manifests/k3-runtime-source-freeze-20260823.json`.

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

## Proposed Moonshine commit sequence

1. **Record the baseline:** source/runtime/evidence hashes and private recovery archives.
2. **Partition unrelated work:** commit or intentionally defer prefix, Q8/static-layout, chat/server, and other features.
3. **Commit MZG1 prerequisites separately:** retain the failed-path reader/transcoder only if still desired for reproducibility.
4. **Commit MZG2 core and builder:** `k3_mzg2.*`, transcode tools, focused build targets, and model-free/CPU validation.
5. **Commit decode integration:** lifecycle, staging, span selection, direct-to-cache admission, copy removal, and post-stream integrity checks.
6. **Commit prefill integration:** temporary-output ownership and exact selected-prefill behavior.
7. **Commit reporting/tests/docs:** profile output, allocation fixtures, instructions, architecture, changelog, and measured evidence.
8. **Qualify a detached clean worktree:** smallest model-free gate, bounded real-weight decoder fixture, engine initialization, then accepted engine fixture after the active workload ends.

## Extraction boundary after Moonshine is clean

The standalone core must not know K3 layer count, expert count, tensor names, manifest hash, environment variables, or cache policy. The K3 adapter retains those responsibilities and must reproduce the accepted Moonshine behavior before any v3 format work begins.
