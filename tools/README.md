# Tools

| Tool | Purpose |
|---|---|
| `mzg2_store_check` | read blocks from an existing store, decode them with the CPU reference, and with `--reencode` require `encode(decode(block)) == block` |
| `mzg2_vectors` | regenerate the committed golden vectors and their manifest |

Neither compiles in a path, a host name or a model identity; everything is an
argument. `mzg2_store_check` only reads.

## Why the re-encode matters

Decoding proves a block parses and its tile checksums hold. A matching
re-encode proves something stronger: that this codec's entropy model,
frequency quantization with its tie-breaks, lane interleave and block layout
are the ones that produced the store. It needs no source checkpoint and no
GPU, which makes it the cheapest strong check available for a derived store.

```sh
./tools/mzg2_store_check kimi-k3 /path/to/expert-store-mzg2 \
    --layer 46 --first 440 --experts 3 --reencode
```

Use `kimi-k3-unbound` to skip the pinned source-manifest check — for a store
derived from a different source revision, where the geometry still applies.
