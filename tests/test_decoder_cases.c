/* Exercise the qualification harness with the CPU backend without ROCm. */
#include "decoder_cases.h"

int main(void)
{
    return mzg2_run_decoder_cases(mzg2_decode_block, NULL, 1u, 0u, 1u);
}
