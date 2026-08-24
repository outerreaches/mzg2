# P0 existing-format implementation plan

This plan changes scheduling and command structure only. It does not change the MZG2 v2 bitstream, numerical contract, model store, cache policy, or default production selection.

## Outcome 2026-08-24

All three P0 candidates were screened against the promoted 128K/30 MZG2 engine
and closed without retained source changes:

- QD1 was slower; QD4 and QD8 did not reproduce a gain over QD2. The
  confirmatory QD4/QD2 pair had identical decode wall (33.013/33.012 s) and
  slower QD4 prompt wall (55.266/52.253 s). **Keep QD2.**
- Layer-scoped integrity clearing preserved exact output but changed mean
  decode wall by only -0.05% and regressed prompt wall 2.77%, below the 1%
  promotion gate. **Reverted.**
- Event-driven prefill buffer reuse preserved exact output but regressed mean
  prompt wall 1.08%, below the 5% gate. **Reverted.**

Artifacts: `moonshine-experiments/mzg2-qd-screen-20260824/`,
`mzg2-layer-clear-screen-20260824/`, and
`mzg2-prefill-event-screen-20260824/`.

## Goals

1. Determine the best compressed-path I/O queue depth.
2. Remove the selected-prefill global synchronization after every expert.
3. Clear MZG2 integrity flags once per layer/batch rather than once per expert.

## Required invariants

- An `O_DIRECT` refill never overwrites a mapped input buffer while GPU decode or GEMM still reads it.
- An MZG2 temporary output is not reused while an expert GEMM still reads it.
- Route-index host memory is not modified until its asynchronous copy is complete.
- Shared device route-index arrays and MoE scratch remain ordered on `expert_stream`.
- A buffer's integrity word is read only after that buffer's completion event.
- Any nonzero integrity word aborts before the buffer is reused or output is published.
- Abort drains outstanding I/O and synchronizes all GPU work before releasing memory.
- Existing raw and MZG1 paths remain correct even if the event scheduler is shared.

## P0a — queue-depth screen

### First implementation

Keep queue depth compile-time for the first screen. Build otherwise-identical candidates with `K3_ENGINE_STREAM_QD` equal to 1, 2, 4, and 8. This automatically sizes local completion/request arrays and MZG2 prefill output slots. Confirm `staging_slots >= stream_qd` at engine creation and update the residency ledger for all temporary outputs.

Do not add a public environment variable until measurement selects a useful value. If decode and prefill prefer different values, split their internal constants only after the first screen.

### Measurements

Use one captured route/workload identity. Record:

- physical read bytes and GiB/s;
- submit, read-wait, expert-pipeline, and synchronization time;
- ring idle intervals;
- per-layer median and p99;
- mapped-memory/GPU contention and NVMe temperature;
- exact output and integrity status.

Adopt only for at least 2% median routed-pipeline improvement without a p99 or thermal regression.

## P0b — selected-prefill event scheduler

### Current serialization

The current completion loop launches one expert, calls `hipStreamSynchronize(expert_stream)`, checks its integrity word, and only then submits a refill into that buffer. This safely owns the two input/output buffers but can leave the ring idle while GPU work runs.

### Buffer state

Track each queue buffer explicitly:

```text
FREE -> IO_IN_FLIGHT -> GPU_IN_FLIGHT -> FREE
```

Each buffer owns:

- its mapped input slot;
- its MZG2 temporary output slot;
- one completion event;
- job identity: layout index, layer, expert, codec, and selected count;
- stable host route-index storage for that in-flight job.

The shared device route-index arrays and MoE scratch may remain shared because every copy/kernel stays ordered on the single `expert_stream`.

### Route-index lifetime

`expert_tokens_host` and `expert_outputs_host` are currently single host arrays. They must not be overwritten while an asynchronous H2D copy may still read them.

Preferred first implementation: allocate host route-index arrays per queue buffer and keep them stable until that buffer's completion event. This is conservative and simple. Account for the extra host bytes explicitly. A later copy-complete event optimization is unnecessary unless memory accounting rejects per-buffer storage.

### Scheduler

1. Clear the active integrity-word range once on `expert_stream`.
2. Submit initial reads into free buffers and mark them `IO_IN_FLIGHT`.
3. On an I/O completion, fill that buffer's route-index arrays.
4. Enqueue MZG2 decode if needed, H2D route indices, gather, expert GEMMs, activation, and scatter on `expert_stream`.
5. Record the buffer's completion event after scatter; mark it `GPU_IN_FLIGHT`.
6. Continue harvesting other I/O completions and enqueue their work without a global stream wait.
7. Before reusing a buffer, wait/query its event, inspect its MZG2 integrity word if applicable, then mark it `FREE` and submit the next read.
8. If no reads are outstanding and all remaining buffers are GPU-busy, wait for the oldest queued event. With one stream, FIFO event order is sufficient.
9. At layer end, reap all GPU-busy buffers, verify integrity, and require zero retained ring operations.

A refill must be associated with the buffer whose event was reaped, not merely the most recent I/O completion.

### Telemetry

The current per-expert host timer includes a global stream wait. After asynchronous scheduling, preserve separately:

- total routed-layer wall time;
- host submit and completion-wait time;
- GPU-busy/event-wait time;
- ring-idle time;
- jobs enqueued and reaped;
- maximum simultaneous I/O- and GPU-owned buffers.

Do not compare old and new `routed_expert_pipeline_seconds` unless its semantic definition remains identical.

### Acceptance

Use a bounded 10–50-expert selected-prefill fixture first. Require:

- exact output;
- successful raw, MZG1-if-retained, and MZG2 buffer ownership tests;
- injected MZG2 corruption aborts before refill/publication;
- no input/output overwrite under event-delay stress;
- no retained I/O or event work on success/failure;
- at least 5% routed-expert wall improvement before broader engine qualification.

## P0c — integrity clear scope

### API change

Change the decoder launch contract from “clear then OR errors” to “OR errors into caller-cleared storage.” Add an explicit bounded clear operation or issue one engine-owned `hipMemsetAsync` for the active error-word range.

### Decode

Before the first MZG2 miss launch in a layer, clear all top-k rank words once on the same stream. After the existing layer stream barrier, inspect only ranks that launched MZG2 decode. Clearing all ranks prevents stale data when hit/miss positions change.

### Prefill

Clear all active queue-buffer words before the first MZG2 job. A completed buffer is inspected before reuse. If it is zero it remains zero; if nonzero, abort immediately. No per-reuse clear is required within that layer.

### Acceptance

Add stale-bit tests across consecutive raw/MZG2, hit/miss, and buffer-reuse patterns. Adopt only if command count falls, all attribution remains correct, and measured layer time improves by at least 1%.

## Patch decomposition

Keep review and rollback simple:

1. Add telemetry and a compile-time QD screen, with no scheduler change.
2. Move integrity clearing to explicit layer/batch scope and add stale-bit tests.
3. Add buffer events and ownership state while retaining QD2.
4. Add per-buffer route-index host storage.
5. Remove the per-expert global stream synchronization.
6. Run bounded ownership/corruption tests, then the selected-prefill gate.
7. Only after QD2 event scheduling passes, repeat the QD sweep.

Do not combine kernel specialization or v3 format changes with this P0 series.
