#ifndef MZG2_TESTS_DECODER_CASES_H
#define MZG2_TESTS_DECODER_CASES_H

#include "mzg2/mzg2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The CPU and HIP runners consume the exact same injected-defect matrix. */
typedef uint32_t (*mzg2_test_decode_fn)(const mzg2_profile *, uint32_t,
    uint32_t, const void *, uint32_t, void *, uint32_t, char *, size_t);

int mzg2_run_decoder_cases(mzg2_test_decode_fn decode, const char *store_path,
                           uint32_t layer, uint32_t first, uint32_t experts);

#ifdef __cplusplus
}
#endif
#endif
