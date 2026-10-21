/* Compile-only by default. An explicit --run starts the GPU qualification. */
#include <hip/hip_runtime.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "decoder_cases.h"
#include "mzg2/decode_hip.h"
#include "mzg2/adapters/kimi_k3.h"

static hipStream_t test_stream;

static void hip_check(hipError_t status, const char *operation)
{
    if (status != hipSuccess) {
        std::fprintf(stderr, "%s: %s\n", operation, hipGetErrorString(status));
        std::exit(1);
    }
}

static uint32_t gpu_decode(const mzg2_profile *profile, uint32_t layer,
    uint32_t expert, const void *block, uint32_t bytes, void *destination,
    uint32_t destination_bytes, char *error, size_t error_size)
{
    constexpr size_t guard = 32u;
    const size_t allocation = profile->output_bytes + guard * 2u;
    void *mapped = nullptr, *device_input = nullptr;
    uint8_t *device_output = nullptr;
    uint32_t *device_flags = nullptr, flags = 0;
    mzg2_block_view view;
    std::vector<uint8_t> readback(allocation);
    const bool valid = mzg2_block_validate(profile, layer, expert, block,
        bytes, &view, error, error_size);
    if (destination_bytes < profile->output_bytes) std::abort();
    hip_check(hipHostMalloc(&mapped, bytes, hipHostMallocMapped), "mapped input");
    std::memcpy(mapped, block, bytes);
    hip_check(hipHostGetDevicePointer(&device_input, mapped, 0), "device input");
    hip_check(hipMalloc(reinterpret_cast<void **>(&device_output), allocation), "destination");
    hip_check(hipMalloc(reinterpret_cast<void **>(&device_flags), sizeof(flags)), "flags");
    hip_check(hipMemsetAsync(device_output, 0xa5, allocation, test_stream), "guards");
    const bool launched = mzg2_hip_decode_launch(profile, layer, expert,
        mapped, device_input, bytes, device_output + guard, device_flags,
        test_stream, error, error_size);
    if (launched != valid) {
        std::fprintf(stderr, "launch disagrees with host validation: %s\n", error);
        std::exit(1);
    }
    hip_check(hipStreamSynchronize(test_stream), "decode completion");
    if (launched) hip_check(hipMemcpy(&flags, device_flags, sizeof(flags),
        hipMemcpyDeviceToHost), "error flags");
    else flags = MZG2_ERROR_BOUNDS;
    hip_check(hipMemcpy(readback.data(), device_output, allocation,
        hipMemcpyDeviceToHost), "readback");
    for (size_t i = 0; i < guard; ++i) {
        if (readback[i] != 0xa5 ||
            readback[guard + profile->output_bytes + i] != 0xa5) {
            std::fprintf(stderr, "destination guard modified\n");
            std::exit(1);
        }
    }
    if (!launched) {
        for (size_t i = guard; i < guard + profile->output_bytes; ++i)
            if (readback[i] != 0xa5) std::abort();
    }
    std::memcpy(destination, readback.data() + guard, profile->output_bytes);
    hip_check(hipFree(device_flags), "free flags");
    hip_check(hipFree(device_output), "free destination");
    hip_check(hipHostFree(mapped), "free mapped input");
    return flags;
}

static void usage()
{
    std::puts("usage: test_hip --run [--store ABSOLUTE_PATH --layer N --first N --experts N]\n"
        "Requires an available wave32 GPU. Defaults: layer=1 first=0 experts=1 (maximum 16).\n"
        "Without --run, the runner requests no device query, allocation or launch.\n"
        "Run from the repository root.");
}

static uint32_t number(const char *value)
{
    char *end;
    errno = 0;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (!*value || *value == '-' || *end || errno || parsed > UINT32_MAX) {
        std::fprintf(stderr, "invalid integer: %s\n", value);
        std::exit(2);
    }
    return static_cast<uint32_t>(parsed);
}

int main(int argc, char **argv)
{
    bool run = false;
    const char *store = nullptr;
    uint32_t layer = 1, first = 0, experts = 1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0) { usage(); return 0; }
        if (std::strcmp(argv[i], "--run") == 0) { run = true; continue; }
        if (i + 1 >= argc) { usage(); return 2; }
        if (std::strcmp(argv[i], "--store") == 0) store = argv[++i];
        else if (std::strcmp(argv[i], "--layer") == 0) layer = number(argv[++i]);
        else if (std::strcmp(argv[i], "--first") == 0) first = number(argv[++i]);
        else if (std::strcmp(argv[i], "--experts") == 0) experts = number(argv[++i]);
        else { usage(); return 2; }
    }
    if (!run) { usage(); return 2; }
    const mzg2_profile *profile = mzg2_kimi_k3_profile();
    if (layer == 0 || layer > profile->max_layers || experts == 0 || experts > 16 ||
        first >= profile->experts_per_layer || experts > profile->experts_per_layer - first ||
        (store && store[0] != '/')) { usage(); return 2; }
    int device = 0;
    hipDeviceProp_t properties;
    hip_check(hipGetDevice(&device), "get device");
    hip_check(hipGetDeviceProperties(&properties, device), "device properties");
    if (properties.warpSize != MZG2_LANES) {
        std::fprintf(stderr, "unsupported wave size: %d\n", properties.warpSize);
        return 1;
    }
    std::printf("GPU=%s ISA=%s wave=%d\n", properties.name,
                properties.gcnArchName, properties.warpSize);
    hip_check(hipStreamCreate(&test_stream), "test stream");
    int result = mzg2_run_decoder_cases(gpu_decode, store, layer, first, experts);
    hip_check(hipStreamDestroy(test_stream), "destroy test stream");
    return result;
}
