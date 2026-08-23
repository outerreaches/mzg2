# Security policy

MZG2 is not yet released and has no supported versions.

The reference implementation assumes trusted local model stores. Decoder errors invalidate the entire destination buffer, including bytes written before the error was reported. The current K3 checksum is designed for accidental corruption detection and is not cryptographic.

Before public release, the project must define its hostile-input boundary, strengthen or explicitly retain the checksum contract, and test malformed headers, models, descriptors, offsets, rANS states, terminal cursors, raw escapes, truncation, and output aliasing.
