/* Helpers shared inside the core. Not a public interface. */
#ifndef MZG2_INTERNAL_H
#define MZG2_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mzg2/mzg2.h"

void mzg2_set_error(char *error, size_t error_size, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/* True when a plane of this kind may code `value`. */
bool mzg2_value_allowed(const mzg2_model_spec *spec, uint32_t value);

static inline uint64_t mzg2_align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1u) / alignment * alignment;
}

/* Symbols a plane packs into one byte: 2 for 4-bit symbols, 1 for 8-bit. */
static inline uint32_t mzg2_symbols_per_byte(uint8_t symbol_bits)
{
    return symbol_bits == 4u ? 2u : 1u;
}

#endif /* MZG2_INTERNAL_H */
