# MZG2 format — pre-normative notes

This is not yet a stable specification.

The K3 reference uses a versioned sidecar index and independently addressable expert blocks. Each block contains:

- identity and geometry header;
- one or more static probability models;
- fixed-output tile descriptors;
- independently bounded tile payloads;
- raw-storage escape;
- expected tile checksums;
- alignment padding suitable for direct I/O.

The selected K3 profile uses 16 KiB output tiles, 32 interleaved rANS states, 12-bit model precision, nibble symbols for packed E2M1 planes, and byte symbols for E8M0 scale planes.

Before this document becomes normative it must define:

- byte order and integer encoding;
- required alignment and permitted padding;
- model-table validation;
- overflow-safe range rules;
- descriptor ordering and overlap rules;
- state initialization, renormalization, and terminal invariants;
- checksum algorithm and threat model;
- raw escape semantics;
- version compatibility and rejection behavior;
- whether model-specific transforms are represented in the core format or adapter metadata.
