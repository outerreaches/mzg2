# Building and checking a K3 store

MZG2 publishes a block encoder and a store reader. The complete K3 checkpoint
transcoder remains in Moonshine. This is the public route until a standalone
store writer is extracted; it does not require redistribution of derived
model blocks in this repository.

The commands below are pinned to public Moonshine commit
[`79b2919010f0ad260e2a8f80ac872368a25a03fd`](https://github.com/outerreaches/moonshine/tree/79b2919010f0ad260e2a8f80ac872368a25a03fd).
They were checked against that source and its
[getting-started guide](https://github.com/outerreaches/moonshine/blob/79b2919010f0ad260e2a8f80ac872368a25a03fd/docs/getting-started.md).
This documentation pass did not rebuild a full store or rerun qualification
of that Moonshine commit.

## Inputs and capacity

- Obtain the official `moonshotai/Kimi-K3` checkpoint under its original
  license, revision `9f62e4e9fffbd0a83ddd60e1c209d828994b3569`.
- Preserve its 96 source SafeTensors shards. The K3 adapter pins source
  manifest SHA-256
  `476fa0ba64e3233cbb9ca0642327361a73f6807e751edb071c92fa2216b202a4`.
- Budget the approximately 1.42 TiB source tree, 1.18 TiB derived output,
  and additional working space. A full build and independent verification
  read the model corpus and contend with serving I/O. Schedule that work
  separately from latency-sensitive workloads.
- Follow the pinned Moonshine build dependencies: ROCm/HIP toolchain, C/C++
  compiler, Make, Python, and its documented system libraries. The `.cu`
  transcoder is built with HIP; the store-construction algorithm is CPU code.

## Build and resume

In a new source checkout, with the model and output paths chosen explicitly:

```sh
git clone https://github.com/outerreaches/moonshine.git moonshine-transcoder
cd moonshine-transcoder
git checkout --detach 79b2919010f0ad260e2a8f80ac872368a25a03fd
make -j1 tools/transcode_mzg2_layer
./tools/transcode_mzg2_full.py \
  --model /path/to/Kimi-K3 \
  --out /path/to/expert-store-mzg2 \
  --jobs 1
```

Choose parallelism for an available machine. The single-worker example is
deliberate; it is not a recommendation to start a full transcode during an
active workload. The builder writes a `.partial` root, rehashes completed
sidecars on resume, independently decodes stored experts against canonicalized
source tensors, and publishes the final root only after all 82,432 experts
pass. Repeat the same command to resume; do not manually rename a partial root.

## Check with this repository

From the MZG2 repository root:

```sh
make -j1 tools
./tools/mzg2_store_check kimi-k3 /path/to/expert-store-mzg2 \
  --layer 46 --first 440 --experts 3 --reencode
```

The `kimi-k3` profile enforces the source binding. This command checks only
the selected experts; it is not full-corpus verification or GPU qualification.
Re-encoding checks the compressed bytes as well as decoded values. Use the
unbound profile only for explicit geometry experiments, never to establish
release qualification or silently accept a different source revision.

K3 canonicalizes MXFP4 negative zero before encoding. The claim is exact
recovery of that canonical representation, not byte identity to arbitrary
source negative-zero encodings. Generated stores remain model weights under
their original license and are not codec-release artifacts.
