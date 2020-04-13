#include "minideen_common.h"
#include <algorithm>
#include <cmath>

enum PathType {
  Slow, Fast
};

__m128 _mm_rcpnr_ps(const __m128 &a) {
  const __m128 r = _mm_rcp_ps(a);
  return _mm_sub_ps(_mm_add_ps(r, r), _mm_mul_ps(_mm_mul_ps(r, a), r));
}

template <PathType pt>
void core_8(const uint8_t *srcp, uint8_t *dstp, int y, int height, int stride, __m128i &bytes_th, int diff_l, int diff_r, int radius) {
  alignas(64) uint8_t border_check[64] = {};

  if constexpr (pt == Slow) {
    for (int i = 0; i < 64; i++)
      if (i - radius >= -diff_l && i - radius < diff_r)
        border_check[i] = 0xFF;
  }

  __m128i center_pixel = _mm_load_si128((const __m128i *)srcp);

  __m128i center_lo = _mm_unpacklo_epi8(center_pixel, zeroes);
  __m128i center_hi = _mm_unpackhi_epi8(center_pixel, zeroes);

  __m128i sum_lo = _mm_slli_epi16(center_lo, 1);
  __m128i sum_hi = _mm_slli_epi16(center_hi, 1);

  __m128i counter = _mm_set1_epi8(2);

  int yyT = std::max(-y, -radius);
  int yyB = std::min(radius, height - y - 1);

  for (int yy = yyT; yy <= yyB; yy++) {
    for (int xx = -radius; xx <= radius; xx++) {
      __m128i neighbour_pixel = _mm_loadu_si128((const __m128i *)(srcp + yy * stride + xx));

      __m128i abs_diff = _mm_or_si128(_mm_subs_epu8(center_pixel, neighbour_pixel),
                      _mm_subs_epu8(neighbour_pixel, center_pixel));

      // Absolute difference less than or equal to th - 1 will be all zeroes.
      abs_diff = _mm_subs_epu8(abs_diff, bytes_th);

      // 0 bytes become 255, not 0 bytes become 0.
      __m128i mask = _mm_cmpeq_epi8(abs_diff, zeroes);

      if constexpr (pt == Slow) {
        __m128i m_border_check = _mm_loadu_si128((const __m128i *)(border_check+radius+xx));
        mask = _mm_and_si128(mask, m_border_check);
      }

      // Subtract 255 aka -1
      counter = _mm_sub_epi8(counter, mask);

      __m128i pixels = _mm_and_si128(mask, neighbour_pixel);

      sum_lo = _mm_adds_epu16(sum_lo,
                  _mm_unpacklo_epi8(pixels, zeroes));
      sum_hi = _mm_adds_epu16(sum_hi,
                  _mm_unpackhi_epi8(pixels, zeroes));
    }
  }

  __m128i counter_lo = _mm_unpacklo_epi8(counter, zeroes);
  __m128i counter_hi = _mm_unpackhi_epi8(counter, zeroes);

  __m128 counter_1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(counter_lo, zeroes));
  __m128 counter_2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(counter_lo, zeroes));
  __m128 counter_3 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(counter_hi, zeroes));
  __m128 counter_4 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(counter_hi, zeroes));

  __m128 sum_1 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(sum_lo, zeroes));
  __m128 sum_2 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(sum_lo, zeroes));
  __m128 sum_3 = _mm_cvtepi32_ps(_mm_unpacklo_epi16(sum_hi, zeroes));
  __m128 sum_4 = _mm_cvtepi32_ps(_mm_unpackhi_epi16(sum_hi, zeroes));

  __m128 resultf_1 = _mm_mul_ps(sum_1, _mm_rcpnr_ps(counter_1));
  __m128 resultf_2 = _mm_mul_ps(sum_2, _mm_rcpnr_ps(counter_2));
  __m128 resultf_3 = _mm_mul_ps(sum_3, _mm_rcpnr_ps(counter_3));
  __m128 resultf_4 = _mm_mul_ps(sum_4, _mm_rcpnr_ps(counter_4));

  // Add 0.5f for rounding.
  resultf_1 = _mm_add_ps(resultf_1, _mm_set1_ps(0.501f));
  resultf_2 = _mm_add_ps(resultf_2, _mm_set1_ps(0.501f));
  resultf_3 = _mm_add_ps(resultf_3, _mm_set1_ps(0.501f));
  resultf_4 = _mm_add_ps(resultf_4, _mm_set1_ps(0.501f));

  __m128i result_1 = _mm_cvttps_epi32(resultf_1);
  __m128i result_2 = _mm_cvttps_epi32(resultf_2);
  __m128i result_3 = _mm_cvttps_epi32(resultf_3);
  __m128i result_4 = _mm_cvttps_epi32(resultf_4);

  __m128i result_lo = _mm_packs_epi32(result_1, result_2);
  __m128i result_hi = _mm_packs_epi32(result_3, result_4);

  _mm_store_si128((__m128i *)dstp, _mm_packus_epi16(result_lo, result_hi));
}

template <PathType pt>
void core_16(const uint16_t *srcp, uint16_t *dstp, int y, int height, int stride, __m128i &words_th, int diff_l, int diff_r, int radius) {
  alignas(64) uint16_t border_check[64] = {};

  if constexpr (pt == Slow) {
    for (int i = 0; i < 64; i++)
      if (i - radius >= -diff_l && i - radius < diff_r)
        border_check[i] = 0xFFFF;
  }

  __m128i center_pixel = _mm_load_si128((const __m128i *)srcp);

  __m128i center_lo = _mm_unpacklo_epi16(center_pixel, zeroes);
  __m128i center_hi = _mm_unpackhi_epi16(center_pixel, zeroes);

  __m128i sum_lo = _mm_slli_epi32(center_lo, 1);
  __m128i sum_hi = _mm_slli_epi32(center_hi, 1);

  __m128i counter = _mm_set1_epi16(2);

  int yyT = std::max(-y, -radius);
  int yyB = std::min(radius, height - y - 1);

  for (int yy = yyT; yy <= yyB; yy++) {
    for (int xx = -radius; xx <= radius; xx++) {
      __m128i neighbour_pixel = _mm_loadu_si128((const __m128i *)(srcp + yy * stride + xx));

      __m128i abs_diff = _mm_or_si128(_mm_subs_epu16(center_pixel, neighbour_pixel),
                      _mm_subs_epu16(neighbour_pixel, center_pixel));

      // Absolute difference less than or equal to th - 1 will be all zeroes.
      abs_diff = _mm_subs_epu16(abs_diff, words_th);

      // 0 words become 65535, not 0 words become 0.
      __m128i mask = _mm_cmpeq_epi16(abs_diff, zeroes);

      if constexpr (pt == Slow) {
        __m128i m_border_check = _mm_loadu_si128((const __m128i *)(border_check+radius+xx));
        mask = _mm_and_si128(mask, m_border_check);
      }

      // Subtract 65535 aka -1
      counter = _mm_sub_epi16(counter, mask);

      __m128i pixels = _mm_and_si128(mask, neighbour_pixel);

      sum_lo = _mm_add_epi32(sum_lo,
                    _mm_unpacklo_epi16(pixels, zeroes));
      sum_hi = _mm_add_epi32(sum_hi,
                    _mm_unpackhi_epi16(pixels, zeroes));
    }
  }

  __m128 counter_lo = _mm_cvtepi32_ps(_mm_unpacklo_epi16(counter, zeroes));
  __m128 counter_hi = _mm_cvtepi32_ps(_mm_unpackhi_epi16(counter, zeroes));

  __m128 resultf_lo = _mm_mul_ps(_mm_cvtepi32_ps(sum_lo), _mm_rcpnr_ps(counter_lo));
  __m128 resultf_hi = _mm_mul_ps(_mm_cvtepi32_ps(sum_hi), _mm_rcpnr_ps(counter_hi));

  // Add 0.5f for rounding.
  resultf_lo = _mm_add_ps(resultf_lo, _mm_set1_ps(0.501f));
  resultf_hi = _mm_add_ps(resultf_hi, _mm_set1_ps(0.501f));

  __m128i result_lo = _mm_cvttps_epi32(resultf_lo);
  __m128i result_hi = _mm_cvttps_epi32(resultf_hi);

  _mm_store_si128((__m128i *)dstp, _mm_packus_epi32(result_lo, result_hi));
}

void minideen_SSE2_8(const uint8_t *srcp, uint8_t *dstp, int width, int height, int src_stride, int dst_stride, unsigned threshold, int radius)
{
  const uint8_t *srcp_orig = srcp;
  uint8_t *dstp_orig = dstp;

  // Subtract 1 so we can use a less than or equal comparison instead of less than.
  __m128i bytes_th = _mm_set1_epi8(threshold - 1);

  const int step = 16;

  // Skip radius pixels on the left and at least radius pixels on the right.
  int width_simd = width & -step;

  int fast_path_l = (radius | (step - 1)) + 1;
  int fast_path_r = (width - radius) & -step;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < fast_path_l; x += step)
      core_8<Slow>(srcp+x, dstp+x, y, height, src_stride, bytes_th, x, width - x, radius);
    for (int x = fast_path_l; x < fast_path_r; x += step)
      core_8<Fast>(srcp+x, dstp+x, y, height, src_stride, bytes_th, 0, 0, radius);
    for (int x = fast_path_r; x < width; x += step)
      core_8<Slow>(srcp+x, dstp+x, y, height, src_stride, bytes_th, x, width - x, radius);

    srcp += src_stride;
    dstp += dst_stride;
  }
}

void minideen_SSE2_16(const uint8_t *srcp8, uint8_t *dstp8, int width, int height, int src_stride, int dst_stride, unsigned threshold, int radius)
{
  const uint16_t *srcp = reinterpret_cast<const uint16_t *>(srcp8);
  uint16_t *dstp = reinterpret_cast<uint16_t *>(dstp8);
  src_stride /= 2;
  dst_stride /= 2;

  // Subtract 1 so we can use a less than or equal comparison instead of less than.
  __m128i words_th = _mm_set1_epi16(threshold - 1);

  const int step = 8;

  int fast_path_l = (radius | (step - 1)) + 1;
  int fast_path_r = (width - radius) & -step;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < fast_path_l; x += step)
      core_16<Slow>(srcp+x, dstp+x, y, height, src_stride, words_th, x, width - x, radius);
    for (int x = fast_path_l; x < fast_path_r; x += step)
      core_16<Fast>(srcp+x, dstp+x, y, height, src_stride, words_th, 0, 0, radius);
    for (int x = fast_path_r; x < width; x += step)
      core_16<Slow>(srcp+x, dstp+x, y, height, src_stride, words_th, x, width - x, radius);

    srcp += src_stride;
    dstp += dst_stride;
  }
}
