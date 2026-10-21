# Benchmarks

Planned benchmarks cover mapped-host input, direct device output, tile-size sweeps, isolated decode, concurrent representative expert compute, corruption rejection, and launch-size scaling. Results must identify GPU ISA, ROCm version, input geometry, physical and logical byte rates, and whether timing includes the complete admission path.

The scripts here analyse logs from the qualified engine. The decoder
benchmarks described above are still planned: they need the HIP decoder run
(see `docs/roadmap.md` Phase 2), and a decode rate measured without the
surrounding engine cannot promote a codec change on its own — the performance
gate is a full-engine A/B (`docs/architecture.md`).
