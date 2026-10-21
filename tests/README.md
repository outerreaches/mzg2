# Tests

Model-free. No GPU, no ROCm, no model weights, no network, no fixed paths:
`make test` runs everywhere the library builds.

| Binary | Checks | Covers |
|---|---:|---|
| `test_format` | 99 | ABI sizes, profile rules, model build/validate, adapter geometry, the zero-gauge transform |
| `test_codec` | 69 | roundtrip, coded and raw tiles, determinism, re-encode fixed point, the K3 geometry on a synthetic payload |
| `test_malformed` | 45 | one injected defect per rule: header, model table, descriptor, integrity, terminal invariants |
| `test_store` | 59 | span and read paths, plus 30 rejection cases on a store the test builds in a temporary directory |
| `test_vectors` | 44 | the committed golden vectors decode, match the catalogue and re-encode byte-identically |

Every rejection case starts from a block or store that passes and changes one
thing, so each gate has been seen to fail. A validator that has never failed
is not evidence that it works.

## Fixtures

`fixture.h` is a synthetic profile — small geometry, its own magic, no source
binding — with a restricted 4-bit alphabet beside an 8-bit one, so the core's
generality is exercised without a model licence. Payloads come from a fixed
seed and mix compressible regions with incompressible ones, so both the coded
path and the raw escape appear in every run.

`sidecar.h` writes sidecars for the store tests. The core is a reader;
building a real store is an adapter/tool concern.

## Golden vectors

`vectors/` holds three committed blocks and a manifest. They pin the
bitstream: if a codec change alters one byte, `test_vectors` fails, and the
fix is either to revert or to regenerate (`make vectors`) with the diff as the
record that the format moved.

An independent implementation can decode these bytes and compare against a
payload it reproduces from the description in `vectors.h`.
