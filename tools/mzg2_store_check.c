/*
 * mzg2_store_check: read blocks from an existing store, decode them with the
 * CPU reference codec, and optionally re-encode the decoded payload and
 * compare it with the stored bytes.
 *
 * Why the re-encode matters: the decoder alone proves a block parses and its
 * tile checksums hold. `encode(decode(block)) == block` proves something
 * stronger -- that the extracted encoder's entropy model, frequency
 * quantization, lane interleave and block layout are the ones that produced
 * the store. That is the extraction's byte-level evidence, and it needs no
 * source checkpoint and no GPU.
 *
 * No store path, host name or model identity is compiled in; everything
 * comes from arguments. The tool reads; it never writes to the store.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mzg2/adapters/kimi_k3.h"
#include "mzg2/mzg2.h"

typedef struct {
    const mzg2_profile *profile;
    const mzg2_plane *planes;
    uint32_t plane_count;
    const char *name;
} adapter;

static bool resolve_adapter(const char *name, adapter *out)
{
    if (strcmp(name, "kimi-k3") == 0) {
        out->profile = mzg2_kimi_k3_profile();
        out->planes = mzg2_kimi_k3_planes(&out->plane_count);
        out->name = "kimi-k3 (source-bound)";
        return true;
    }
    if (strcmp(name, "kimi-k3-unbound") == 0) {
        out->profile = mzg2_kimi_k3_profile_unbound();
        out->planes = mzg2_kimi_k3_planes(&out->plane_count);
        out->name = "kimi-k3 (unbound)";
        return true;
    }
    return false;
}

static double seconds_since(const struct timespec *start)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - start->tv_sec) +
           (double)(now.tv_nsec - start->tv_nsec) / 1e9;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s ADAPTER STORE_PATH [options]\n"
            "  ADAPTER      kimi-k3 | kimi-k3-unbound\n"
            "  STORE_PATH   absolute path to a sidecar file or a store "
            "directory\n"
            "options:\n"
            "  --layer N    layer to sample (default: the first present)\n"
            "  --experts N  experts to check from that layer (default 4)\n"
            "  --first N    first expert index (default 0)\n"
            "  --reencode   also require encode(decode(block)) == block\n",
            program);
}

int main(int argc, char **argv)
{
    adapter selected;
    mzg2_store *store = NULL;
    uint8_t *block = NULL;
    uint8_t *payload = NULL;
    char error[512];
    const char *path;
    uint32_t layer = 0u;
    uint32_t experts = 4u;
    uint32_t first = 0u;
    bool reencode = false;
    uint32_t checked = 0u;
    uint32_t failures = 0u;
    uint64_t stored_bytes = 0u;
    uint32_t max_block;
    int argument;

    memset(&selected, 0, sizeof(selected));
    if (argc < 3 || !resolve_adapter(argv[1], &selected)) {
        usage(argv[0]);
        return 2;
    }
    path = argv[2];
    for (argument = 3; argument < argc; ++argument) {
        const bool has_value = argument + 1 < argc;

        if (strcmp(argv[argument], "--reencode") == 0) {
            reencode = true;
        } else if (strcmp(argv[argument], "--layer") == 0 && has_value) {
            layer = (uint32_t)strtoul(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--experts") == 0 && has_value) {
            experts = (uint32_t)strtoul(argv[++argument], NULL, 10);
        } else if (strcmp(argv[argument], "--first") == 0 && has_value) {
            first = (uint32_t)strtoul(argv[++argument], NULL, 10);
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!mzg2_store_open(selected.profile, path, &store, error,
                         sizeof(error))) {
        fprintf(stderr, "store open failed: %s\n", error);
        return 1;
    }
    max_block = mzg2_store_max_block_bytes(store);
    printf("adapter: %s\n", selected.name);
    printf("experts present: %llu\n",
           (unsigned long long)mzg2_store_present_count(store));
    printf("largest block: %u bytes\n", max_block);

    if (layer == 0u) {
        uint32_t candidate;

        for (candidate = 1u; candidate <= selected.profile->max_layers;
             ++candidate) {
            mzg2_span span;

            if (mzg2_store_span(store, candidate, first, &span)) {
                layer = candidate;
                break;
            }
        }
        if (layer == 0u) {
            fprintf(stderr, "no layer holds expert %u\n", first);
            mzg2_store_destroy(store);
            return 1;
        }
    }

    block = (uint8_t *)malloc(max_block);
    payload = (uint8_t *)malloc(selected.profile->output_bytes);
    if (!block || !payload) {
        fprintf(stderr, "allocation failed\n");
        free(block);
        free(payload);
        mzg2_store_destroy(store);
        return 1;
    }

    printf("checking layer %u, experts %u..%u%s\n", layer, first,
           first + experts - 1u, reencode ? ", with re-encode" : "");
    {
        struct timespec start;
        uint32_t expert;

        clock_gettime(CLOCK_MONOTONIC, &start);
        for (expert = first; expert < first + experts; ++expert) {
            uint32_t block_bytes = 0u;
            uint32_t flags;

            if (!mzg2_store_read_block(store, layer, expert, block, max_block,
                                       &block_bytes, error, sizeof(error))) {
                printf("  expert %u: READ FAILED: %s\n", expert, error);
                ++failures;
                continue;
            }
            flags = mzg2_decode_block(selected.profile, layer, expert, block,
                                      block_bytes, payload,
                                      selected.profile->output_bytes, error,
                                      sizeof(error));
            if (flags != 0u) {
                printf("  expert %u: DECODE FAILED flags=%u: %s\n", expert,
                       flags, error);
                ++failures;
                continue;
            }
            if (reencode) {
                mzg2_built_model models[MZG2_MAX_MODELS];
                mzg2_model_entry entry;
                mzg2_encoded again;

                if (!mzg2_models_from_planes(selected.profile, payload,
                                             selected.planes,
                                             selected.plane_count, models,
                                             &entry, error, sizeof(error))) {
                    printf("  expert %u: MODEL BUILD FAILED: %s\n", expert,
                           error);
                    ++failures;
                    continue;
                }
                if (!mzg2_encode_block(selected.profile, layer, expert,
                                       payload, selected.planes,
                                       selected.plane_count, models, &entry,
                                       &again, error, sizeof(error))) {
                    printf("  expert %u: RE-ENCODE FAILED: %s\n", expert,
                           error);
                    ++failures;
                    continue;
                }
                if (again.size != block_bytes ||
                    memcmp(again.bytes, block, block_bytes) != 0) {
                    uint32_t offset = 0u;

                    while (offset < again.size && offset < block_bytes &&
                           again.bytes[offset] == block[offset]) {
                        ++offset;
                    }
                    printf("  expert %u: RE-ENCODE DIFFERS: %u vs %u bytes, "
                           "first difference at %u\n",
                           expert, again.size, block_bytes, offset);
                    ++failures;
                    mzg2_encoded_release(&again);
                    continue;
                }
                printf("  expert %u: decode + re-encode identical (%u bytes, "
                       "%u raw tiles)\n",
                       expert, block_bytes, again.raw_tiles);
                mzg2_encoded_release(&again);
            } else {
                printf("  expert %u: decode ok (%u bytes)\n", expert,
                       block_bytes);
            }
            stored_bytes += block_bytes;
            ++checked;
        }
        printf("checked %u experts in %.2f s, %u failures\n", checked,
               seconds_since(&start), failures);
    }
    if (checked != 0u) {
        const double raw =
            (double)checked * (double)selected.profile->output_bytes;

        printf("sampled reduction: %.4f%% (%llu stored of %.0f raw bytes)\n",
               100.0 * (1.0 - (double)stored_bytes / raw),
               (unsigned long long)stored_bytes, raw);
    }
    free(block);
    free(payload);
    mzg2_store_destroy(store);
    return failures == 0u ? 0 : 1;
}
