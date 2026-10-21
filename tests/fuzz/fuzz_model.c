/* Reach model validation directly, without the outer block header gate. */
#include "fixture.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    mzg2_model table;
    mzg2_built_model built;
    uint64_t counts[256] = {0};
    mzg2_model_spec spec = {8u, NULL};
    size_t i;

    if (size > 65536u) return 0;
    memset(&table, 0, sizeof(table));
    if (size) memcpy(&table, data, size < sizeof(table) ? size : sizeof(table));
    (void)mzg2_model_validate(&table, &spec);
    (void)mzg2_model_validate(&table, &mzg2_fixture_profile()->model[0]);
    for (i = 0; i + sizeof(uint64_t) <= size && i / 8u < 15u; i += 8u)
        memcpy(&counts[i / 8u], data + i, sizeof(uint64_t));
    if (mzg2_model_build(counts, &spec, &built) &&
        !mzg2_model_validate(&built.table, &spec)) abort();
    return 0;
}
