# Kimi K3 adapter

The qualified MZG2 research profile, extracted from the Moonshine
implementation (`c5b65c7` … `c773930`).

Everything model-specific lives here:

- the container magic `K3MZG2` and the `layer-%03u.mzg2` sidecar naming;
- the 92 × 896 expert grid and the 17,547,264-byte decoded expert;
- the six MXFP4 planes — `w1`, `w2`, `w3` × (packed nibbles, E8M0 scales) —
  in the physical order they occupy inside one expert's source span, with
  their tensor-name suffixes;
- the **zero-gauge transform**: E2M1 negative zero folded onto positive zero,
  which is why the packed alphabet has 15 symbols and why the contract is
  dequantized-value-preserving rather than byte-exact;
- the pinned `source_manifest_sha256`, so a store derived from any other
  source revision is rejected at open.

`mzg2_kimi_k3_profile_unbound()` is the same geometry with no source binding,
for synthetic fixtures. It must not be used to open a store that will serve a
model.

Building a store is not here: for K3 that is the Moonshine transcoder, which
reads the official checkpoint. This adapter states the names and the layout a
transcoder needs. See the [pinned build and verification route](../../docs/store-building.md).
