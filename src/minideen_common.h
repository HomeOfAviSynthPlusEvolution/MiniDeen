#include <cstdint>
#include <immintrin.h>

#define zeroes _mm_setzero_si128()

template <typename PixelType>
void minideen_C(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);

void minideen_SSE2_8(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);

void minideen_SSE2_16(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);
