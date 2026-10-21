/*
 * HIP decoder: validate one host-visible block and enqueue its bounded
 * wave32 decode straight into a caller-provided device destination.
 *
 * There is no intermediate buffer and no CPU decode step: `block_device`
 * aliases `block_host` through mapped host memory, and the kernel writes the
 * destination an engine already admitted -- for an expert cache, the slot
 * itself. That is the whole point of the format.
 *
 * Requires a 32-lane wave. The v2 bitstream's renormalization order is
 * defined by 32 lanes consuming words in increasing lane order, so a
 * wave64 target needs its own kernel, not a recompile.
 */
#ifndef MZG2_DECODE_HIP_H
#define MZG2_DECODE_HIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mzg2/mzg2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `block_host` must be readable by the host (it is validated there) and
 * `block_device` must be the device-visible address of the same bytes.
 * `destination` holds at least profile->output_bytes device bytes.
 * `error_device` is a device uint32_t the kernel ORs its flags into; it is
 * cleared on the stream before the launch. `stream` is a hipStream_t passed
 * opaquely so this header stays free of HIP types.
 *
 * Returns false if validation or the launch failed, with a message. A true
 * return means the decode is enqueued, not that it succeeded: read
 * `error_device` after synchronizing, and treat the destination as invalid
 * if it is nonzero.
 */
bool mzg2_hip_decode_launch(const mzg2_profile *profile, uint32_t layer,
                            uint32_t expert, const void *block_host,
                            const void *block_device, uint32_t block_bytes,
                            void *destination, uint32_t *error_device,
                            void *stream, char *error, size_t error_size);

/* Threads per block the launch uses, and waves per block it implies. */
uint32_t mzg2_hip_decode_threads(void);

#ifdef __cplusplus
}
#endif

#endif /* MZG2_DECODE_HIP_H */
