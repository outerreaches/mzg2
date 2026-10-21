/*
 * HIP wave32 tile decoder.
 *
 * Extracted from the qualified Kimi K3 implementation with the model
 * constants replaced by mzg2_tile_rules. The arithmetic, the lane interleave
 * and the error semantics are unchanged: one wave per tile, one symbol per
 * lane per round, a wave vote to find the lanes that need renormalization,
 * and a strict terminal check.
 *
 * The CPU reference codec (core/src/codec.c) is this kernel's oracle. They
 * must agree bit for bit on every block; the golden vectors pin both.
 */
#include <hip/hip_runtime.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mzg2/decode_hip.h"
#include "mzg2/mzg2.h"

namespace {

constexpr uint32_t kWaveSize = MZG2_LANES;
constexpr uint32_t kThreads = 256u;
constexpr uint32_t kWavesPerBlock = kThreads / kWaveSize;

void set_error(char *error, size_t error_size, const char *format, ...)
{
    va_list arguments;

    if (!error || error_size == 0u) {
        return;
    }
    va_start(arguments, format);
    (void)vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
}

__global__ void decode_tiles_kernel(const uint8_t *block, uint32_t block_bytes,
                                    const mzg2_tile_descriptor *descriptors,
                                    const mzg2_model_entry *entry,
                                    uint8_t *output, uint32_t *error_flags,
                                    mzg2_tile_rules rules)
{
    __shared__ uint8_t decode_lut[MZG2_MAX_MODELS][MZG2_MODEL_SLOTS];
    __shared__ uint8_t model_values[MZG2_MAX_MODELS][MZG2_MAX_SYMBOLS];
    __shared__ uint8_t symbol_count[MZG2_MAX_MODELS];
    __shared__ uint32_t frequency[MZG2_MAX_MODELS][MZG2_MAX_SYMBOLS];
    __shared__ uint32_t cumulative[MZG2_MAX_MODELS][MZG2_MAX_SYMBOLS];
    __shared__ mzg2_tile_descriptor tile_descriptors[kWavesPerBlock];

    for (uint32_t index = threadIdx.x;
         index < MZG2_MAX_MODELS * MZG2_MODEL_SLOTS; index += blockDim.x) {
        decode_lut[index / MZG2_MODEL_SLOTS][index % MZG2_MODEL_SLOTS] =
            entry->model[index / MZG2_MODEL_SLOTS]
                .decode[index % MZG2_MODEL_SLOTS];
    }
    for (uint32_t index = threadIdx.x;
         index < MZG2_MAX_MODELS * MZG2_MAX_SYMBOLS; index += blockDim.x) {
        const uint32_t model = index / MZG2_MAX_SYMBOLS;
        const uint32_t symbol = index % MZG2_MAX_SYMBOLS;

        model_values[model][symbol] = entry->model[model].values[symbol];
        frequency[model][symbol] = entry->model[model].frequency[symbol];
        cumulative[model][symbol] = entry->model[model].cumulative[symbol];
    }
    if (threadIdx.x < MZG2_MAX_MODELS) {
        symbol_count[threadIdx.x] = entry->model[threadIdx.x].symbol_count;
    }
    if (threadIdx.x < kWavesPerBlock) {
        const uint32_t index = blockIdx.x * kWavesPerBlock + threadIdx.x;

        if (index < rules.tile_count) {
            tile_descriptors[threadIdx.x] = descriptors[index];
        }
    }
    __syncthreads();

    const uint32_t lane = threadIdx.x & (kWaveSize - 1u);
    const uint32_t wave = threadIdx.x / kWaveSize;
    const uint32_t tile_index = blockIdx.x * kWavesPerBlock + wave;

    if (tile_index >= rules.tile_count) {
        return;
    }
    const mzg2_tile_descriptor descriptor = tile_descriptors[wave];
    const uint32_t model = descriptor.kind & MZG2_MODEL_MASK;
    const uint32_t output_offset = tile_index * rules.tile_bytes;
    const uint32_t expected_output =
        rules.output_bytes - output_offset < rules.tile_bytes
            ? rules.output_bytes - output_offset
            : rules.tile_bytes;

    bool valid =
        model < rules.model_count &&
        (descriptor.kind & ~(MZG2_TILE_RAW | MZG2_MODEL_MASK)) == 0u &&
        descriptor.reserved == 0u && (descriptor.checksum >> 32u) == 0u &&
        descriptor.output_bytes == expected_output &&
        descriptor.output_offset == output_offset &&
        descriptor.output_offset <= rules.output_bytes &&
        descriptor.output_bytes <=
            rules.output_bytes - descriptor.output_offset &&
        descriptor.payload_offset % 4u == 0u &&
        descriptor.payload_offset <= block_bytes &&
        descriptor.payload_bytes <= block_bytes - descriptor.payload_offset;

    if (!valid) {
        if (lane == 0u) {
            atomicOr(error_flags, MZG2_ERROR_BOUNDS);
        }
        return;
    }

    const uint8_t *payload = block + descriptor.payload_offset;
    uint8_t *destination = output + descriptor.output_offset;
    uint32_t checksum = 0u;

    if (descriptor.kind & MZG2_TILE_RAW) {
        if (descriptor.payload_bytes != descriptor.output_bytes) {
            if (lane == 0u) {
                atomicOr(error_flags, MZG2_ERROR_BOUNDS);
            }
            return;
        }
        for (uint32_t offset = lane * 4u; offset < descriptor.output_bytes;
             offset += kWaveSize * 4u) {
            const uint32_t word =
                *reinterpret_cast<const uint32_t *>(payload + offset);

            *reinterpret_cast<uint32_t *>(destination + offset) = word;
            checksum ^= mzg2_word_checksum(word, offset / 4u);
        }
    } else {
        const uint32_t state_bytes = kWaveSize * sizeof(uint32_t);

        if (descriptor.payload_bytes < state_bytes ||
            (descriptor.payload_bytes - state_bytes) % 2u) {
            if (lane == 0u) {
                atomicOr(error_flags, MZG2_ERROR_BOUNDS);
            }
            return;
        }
        const uint32_t *initial_states =
            reinterpret_cast<const uint32_t *>(payload);
        const uint16_t *words =
            reinterpret_cast<const uint16_t *>(payload + state_bytes);
        const uint32_t word_count =
            (descriptor.payload_bytes - state_bytes) / 2u;
        const uint32_t symbol_bits = rules.symbol_bits[model];
        const uint32_t symbols_per_dword = 32u / symbol_bits;
        const uint32_t symbols_per_byte = symbol_bits == 4u ? 2u : 1u;
        const uint32_t rounds =
            descriptor.output_bytes * symbols_per_byte / kWaveSize;
        const unsigned long long lane_mask =
            lane == 0u ? 0u : ((1ull << lane) - 1u);
        uint32_t state = initial_states[lane];
        uint32_t cursor = 0u;
        uint32_t assembled = 0u;

        if (__ballot(state < MZG2_LOWER_BOUND)) {
            valid = false;
        }
        for (uint32_t round = 0u; round < rounds && valid; ++round) {
            const uint32_t slot = state & (MZG2_MODEL_SLOTS - 1u);
            const uint32_t symbol = decode_lut[model][slot];

            if (symbol >= MZG2_MAX_SYMBOLS || symbol >= symbol_count[model]) {
                valid = false;
                break;
            }
            state = frequency[model][symbol] * (state >> MZG2_SCALE_BITS) +
                    slot - cumulative[model][symbol];

            const bool need = state < MZG2_LOWER_BOUND;
            const unsigned long long mask = __ballot(need);
            const uint32_t needed = __popcll(mask);

            if (needed > word_count - cursor) {
                valid = false;
                break;
            }
            if (need) {
                state = (state << 16u) | words[cursor + __popcll(mask & lane_mask)];
            }
            cursor += needed;

            const uint32_t slot_in_dword = round % symbols_per_dword;

            assembled |= model_values[model][symbol] << (slot_in_dword * symbol_bits);
            if (slot_in_dword == symbols_per_dword - 1u) {
                const uint32_t offset =
                    ((round / symbols_per_dword) * kWaveSize + lane) * 4u;

                *reinterpret_cast<uint32_t *>(destination + offset) = assembled;
                checksum ^= mzg2_word_checksum(assembled, offset / 4u);
                assembled = 0u;
            }
        }
        if (__ballot(!valid || cursor != word_count ||
                     state != MZG2_LOWER_BOUND)) {
            if (lane == 0u) {
                atomicOr(error_flags, valid ? MZG2_ERROR_TERMINAL
                                            : MZG2_ERROR_STATE);
            }
            return;
        }
    }
    for (uint32_t delta = kWaveSize / 2u; delta > 0u; delta >>= 1u) {
        checksum ^= __shfl_xor(checksum, delta, kWaveSize);
    }
    if (lane == 0u && checksum != static_cast<uint32_t>(descriptor.checksum)) {
        atomicOr(error_flags, MZG2_ERROR_CHECKSUM);
    }
}

}  // namespace

extern "C" uint32_t mzg2_hip_decode_threads(void) { return kThreads; }

extern "C" bool mzg2_hip_decode_launch(
    const mzg2_profile *profile, uint32_t layer, uint32_t expert,
    const void *block_host, const void *block_device, uint32_t block_bytes,
    void *destination, uint32_t *error_device, void *stream_opaque,
    char *error, size_t error_size)
{
    mzg2_block_view view;
    mzg2_tile_rules rules;

    if (error && error_size) {
        error[0] = '\0';
    }
    if (!profile || !block_host || !block_device || !destination ||
        !error_device) {
        set_error(error, error_size, "invalid MZG2 launch arguments");
        return false;
    }
    /* The same host-side gate the CPU decoder runs, before any device work. */
    if (!mzg2_block_validate(profile, layer, expert, block_host, block_bytes,
                             &view, error, error_size)) {
        return false;
    }
    int device = 0, wave_size = 0;
    hipError_t status = hipGetDevice(&device);
    if (status == hipSuccess) {
        status = hipDeviceGetAttribute(&wave_size, hipDeviceAttributeWarpSize,
                                        device);
    }
    if (status != hipSuccess || wave_size != static_cast<int>(MZG2_LANES)) {
        set_error(error, error_size, "MZG2 requires wave32 (reported %d): %s",
                  wave_size, hipGetErrorString(status));
        return false;
    }
    mzg2_profile_tile_rules(profile, &rules);

    hipStream_t stream = static_cast<hipStream_t>(stream_opaque);
    status = hipMemsetAsync(error_device, 0, sizeof(*error_device), stream);

    if (status != hipSuccess) {
        set_error(error, error_size, "clear MZG2 error flag: %s",
                  hipGetErrorString(status));
        return false;
    }

    const uint8_t *device_bytes = static_cast<const uint8_t *>(block_device);
    const auto *device_models = reinterpret_cast<const mzg2_model_entry *>(
        device_bytes + view.header->model_offset);
    const auto *device_descriptors =
        reinterpret_cast<const mzg2_tile_descriptor *>(
            device_bytes + view.header->descriptor_offset);
    const uint32_t blocks =
        (rules.tile_count + kWavesPerBlock - 1u) / kWavesPerBlock;

    hipLaunchKernelGGL(decode_tiles_kernel, dim3(blocks), dim3(kThreads), 0,
                       stream, device_bytes, block_bytes, device_descriptors,
                       device_models, static_cast<uint8_t *>(destination),
                       error_device, rules);
    status = hipGetLastError();
    if (status != hipSuccess) {
        set_error(error, error_size, "launch MZG2 decode: %s",
                  hipGetErrorString(status));
        return false;
    }
    return true;
}

static_assert(sizeof(mzg2_file_header) == 124u, "MZG2 file header ABI");
static_assert(sizeof(mzg2_index_entry) == 16u, "MZG2 index ABI");
static_assert(sizeof(mzg2_block_header) == 64u, "MZG2 block header ABI");
static_assert(sizeof(mzg2_tile_descriptor) == 32u, "MZG2 descriptor ABI");
static_assert(sizeof(mzg2_model) == 4248u, "MZG2 model ABI");
