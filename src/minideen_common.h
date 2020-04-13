#include <cstdint>
#include <immintrin.h>

enum PathType {
  Slow, Fast
};

template <typename PixelType>
void minideen_C(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);

void minideen_SSE2_8(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);
void minideen_SSE2_16(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);

void minideen_AVX2_8(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);
void minideen_AVX2_16(const uint8_t *, uint8_t *, int, int, int, int, unsigned int, int);
