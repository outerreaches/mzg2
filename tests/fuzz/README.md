# CPU fuzzing

These libFuzzer targets use ASan and UBSan and only synthetic inputs. They
open no model weights and make no HIP calls. Clang with compiler-rt's fuzzer
and sanitizer libraries is required; Python 3 runs the bounded smoke driver.

```sh
make -j1 sanitize CLANG=clang
make -j1 fuzz-smoke CLANG=clang FUZZ_RUNS=2000
```

Use `CLANG=clang-17` or another installed Clang path when there is no unversioned
command. Separate build directories prevent reuse of ordinary objects in an
instrumented build. Each smoke target runs serially, with a fixed seed,
at most 2,000 inputs or 30 seconds by default, a five-second per-input timeout,
a 64 KiB input limit and a 256 MiB RSS limit. The parent also enforces a
60-second wall timeout per target. A limit reached is not exhaustive coverage.

| Target | Surface |
|---|---|
| `fuzz_block` | complete block/header/model/descriptor mutation, decode, destination guards, unchanged output after structural rejection |
| `fuzz_model` | model-table validation plus construction from bounded-count arrays, including 64-bit count extremes |
| `fuzz_roundtrip` | encoder/decoder agreement with varying content, 4/8-bit widths, 128–2048-byte tiles and short final tiles |
| `fuzz_store` | sidecar header/index parsing, bounded store reads and decoding under the synthetic profile |

The seed generator builds valid blocks, models, payloads and a sidecar from
the synthetic fixtures so the fuzzer starts beyond magic/header rejection.
The store target uses private temporary files in `/tmp` and removes them after
each input; the filesystem must support opening an `O_DIRECT` descriptor.
The profile and filesystem are controlled by the harness. This is not
coverage of adversarial paths, concurrent file replacement or every public
API argument.

The runner creates a fresh corpus below ignored `build/fuzz/` and removes it
on completion. Crash artifacts remain in `build/fuzz/artifacts/`; inspect and
minimize them locally, then add a deterministic regression. Do not seed these
targets with real model blocks or publish private corpora. A clean smoke run
does not change the trusted-local-store security boundary.
