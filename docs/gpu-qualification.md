# Extracted HIP decoder qualification

Status: runner prepared; GPU execution remains pending. Building it does not
close the parity gate. No device test is part of `make`, `make test`, CI,
sanitizers or fuzz smoke.

## Prepare without a GPU window

```sh
make -j1 BUILD_DIR=build/hip hip-test-build GPU_ARCH=gfx1151
```

This compiles and links the HIP executable without running it. The CPU suite
also runs `test_decoder_cases`, which exercises the shared qualification
fixtures through the CPU backend; that tests harness plumbing, not HIP parity.

## Run only in an available GPU window

From the repository root, record the exact commit and compiler/ROCm versions
along with the log. Run synthetic parity first:

```sh
./build/hip/tests/test_hip --run
```

The runner prints the GPU ISA and wave size and refuses a non-wave32 device.
The launch API also rejects a non-wave32 device before dispatch. Other wave32
architectures remain unqualified; the target of this gate is `gfx1151`.

The synthetic run covers all three committed golden vectors against both
the CPU decode and regenerated expected payload, then replays the same
injected-defect matrix as `test_malformed`. A valid exact decode follows each
defect. The GPU callback uses mapped host input, a caller-owned device output
and guards; it checks unchanged destination on host rejection, completion
flags and guard integrity before releasing memory. Decode errors invalidate
the entire destination. The matrix includes deterministic STATE, CHECKSUM
and TERMINAL cases; structural cases are rejected before launch.

Then supply a bounded source-bound K3 sample explicitly:

```sh
./build/hip/tests/test_hip --run \
  --store /path/to/expert-store-mzg2 --layer 46 --first 440 --experts 3
```

Repeat selected groups from layers 1 and 92 to match the extraction's sampling
coverage. A run accepts at most 16 experts; no full-corpus operation is
implicit. No `--store` means the real-store gate is visibly **NOT RUN**, even
if the synthetic suite passes. Real blocks are never written by the runner.

The store input is read through the buffered tool API then copied to mapped
staging. This is a correctness test, not an `O_DIRECT` admission benchmark,
compute-contention measurement, or new full-engine performance result.

## Evidence and disposition

Retain candidate commit, binary hash, GPU/ROCm/compiler identity, vector
manifest, selected layer/expert IDs and final exit status. Record synthetic
and real-store outcomes separately. Publish only reviewed aggregate evidence;
keep weights and private paths out of release attachments. Any parity, guard,
launch or error-reporting failure leaves the GPU gate open.
