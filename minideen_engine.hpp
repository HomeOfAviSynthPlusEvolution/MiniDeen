#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined (MINIDEEN_X86)
  #if _MSC_VER
    #include <intrin.h>
  #else
    #include <x86intrin.h>
  #endif
#endif

// The largest is 15x15, but the center pixel gets added three times.
// + 1 because 227 needs to be a valid index in the magic array.
#define MAX_PIXEL_COUNT 227 + 1

class MiniDeenEngine {
protected:

    ;
public:
    template <typename PixelType>
    static void process_plane_scalar(const uint8_t *srcp8, uint8_t *dstp8, int first_column, int last_column, int width, int height, int stride, int stride_w, unsigned threshold, int radius, const uint16_t magic[MAX_PIXEL_COUNT]) {
        (void)magic;

        const PixelType *srcp = (const PixelType *)srcp8;
        PixelType *dstp = (PixelType *)dstp8;
        stride /= sizeof(PixelType);
        stride_w /= sizeof(PixelType);

        for (int y = 0; y < height; y++) {
            for (int x = first_column; x < last_column; x++) {
                unsigned center_pixel = srcp[x];

                unsigned sum = center_pixel * 2;
                unsigned counter = 2;

                for (int yy = std::max(-y, -radius); yy <= std::min(radius, height - y - 1); yy++) {
                    for (int xx = std::max(-x, -radius); xx <= std::min(radius, width - x - 1); xx++) {
                        unsigned neighbour_pixel = srcp[x + yy * stride + xx];

                        if (threshold > (unsigned)std::abs((int)center_pixel - (int)neighbour_pixel)) {
                            counter++;
                            sum += neighbour_pixel;
                        }
                    }
                }

                dstp[x] = (sum * 2 + counter) / (counter * 2);
            }

            srcp += stride;
            dstp += stride_w;
        }
    }


#if defined (MINIDEEN_X86)

#define zeroes _mm_setzero_si128()


    static void process_plane_sse2_8bit(const uint8_t *srcp, uint8_t *dstp, int first_column, int last_column, int width, int height, int stride, int stride_w, unsigned threshold, int radius, const uint16_t magic[MAX_PIXEL_COUNT]) {
        (void)first_column; // Unused in this function.
        (void)last_column; // Unused in this function.

        const uint8_t *srcp_orig = srcp;
        uint8_t *dstp_orig = dstp;

        uint16_t magic_buffer[8];

        // Subtract 1 so we can use a less than or equal comparison instead of less than.
        __m128i bytes_th = _mm_set1_epi8(threshold - 1);

        const int pixels_in_xmm = 16;

        // Skip radius pixels on the left and at least radius pixels on the right.
        int width_simd = width & -pixels_in_xmm;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width_simd; x += pixels_in_xmm) {
                __m128i center_pixel = _mm_load_si128((const __m128i *)&srcp[x]);

                __m128i center_lo = _mm_unpacklo_epi8(center_pixel, zeroes);
                __m128i center_hi = _mm_unpackhi_epi8(center_pixel, zeroes);

                __m128i sum_lo = _mm_slli_epi16(center_lo, 1);
                __m128i sum_hi = _mm_slli_epi16(center_hi, 1);

                __m128i counter = _mm_set1_epi8(2);

                for (int yy = std::max(-y, -radius); yy <= std::min(radius, height - y - 1); yy++) {
                    for (int xx = -radius; xx <= radius; xx++) {
                        __m128i neighbour_pixel = _mm_loadu_si128((const __m128i *)&srcp[x + yy * stride + xx]);

                        __m128i abs_diff = _mm_or_si128(_mm_subs_epu8(center_pixel, neighbour_pixel),
                                                        _mm_subs_epu8(neighbour_pixel, center_pixel));

                        // Absolute difference less than or equal to th - 1 will be all zeroes.
                        abs_diff = _mm_subs_epu8(abs_diff, bytes_th);

                        // 0 bytes become 255, not 0 bytes become 0.
                        __m128i mask = _mm_cmpeq_epi8(abs_diff, zeroes);

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

                sum_lo = _mm_add_epi16(sum_lo,
                                       _mm_srli_epi16(counter_lo, 1));
                sum_hi = _mm_add_epi16(sum_hi,
                                       _mm_srli_epi16(counter_hi, 1));

                _mm_storeu_si128((__m128i *)&magic_buffer, counter_lo);
                for (int i = 0; i < 8; i++)
                    magic_buffer[i] = magic[magic_buffer[i]];
                __m128i magic_lo = _mm_loadu_si128((__m128i *)&magic_buffer);

                _mm_storeu_si128((__m128i *)&magic_buffer, counter_hi);
                for (int i = 0; i < 8; i++)
                    magic_buffer[i] = magic[magic_buffer[i]];
                __m128i magic_hi = _mm_loadu_si128((__m128i *)&magic_buffer);

                __m128i result_lo = _mm_mulhi_epu16(sum_lo, magic_lo);
                __m128i result_hi = _mm_mulhi_epu16(sum_hi, magic_hi);

                _mm_storeu_si128((__m128i *)&dstp[x],
                                 _mm_packus_epi16(result_lo, result_hi));
            }
            /// sum / counter
            /// there is a number which, when multiplied by counter, results in 65536 (approximately)
            /// so multiply sum by the same number, and shift the result right 16 bits
            /// magic number: 65536 / counter
            ///
            /// (sum * magic + 32768) >> 16
            ///
            /// (sum * (65536 / counter) + 32768) >> 16
            /// (sum * 65536 / counter + 32768) >> 16
            /// (sum * 65536 / counter + 32768 * counter / counter) >> 16
            /// ((sum * 65536 + counter * 32768) / counter) >> 16
            /// ((sum * 2 * 32768 + counter * 32768) / counter) >> 16
            /// ((sum * 2 + counter) * 32768 / counter) >> 16
            /// ((sum * 2 + half_counter * 2) * 32768 / counter) >> 16
            /// (sum + half_counter) * 65536 / counter) >> 16

            srcp += stride;
            dstp += stride_w;
        }

        process_plane_scalar<uint8_t>(srcp_orig,
                                      dstp_orig,
                                      0,
                                      radius,
                                      width,
                                      height,
                                      stride,
                                      stride_w,
                                      threshold,
                                      radius,
                                      magic);

        process_plane_scalar<uint8_t>(srcp_orig,
                                      dstp_orig,
                                      std::min(width_simd, width - radius),
                                      width,
                                      width,
                                      height,
                                      stride,
                                      stride_w,
                                      threshold,
                                      radius,
                                      magic);
    }


    static void process_plane_sse2_16bit(const uint8_t *srcp8, uint8_t *dstp8, int first_column, int last_column, int width, int height, int stride, int stride_w, unsigned threshold, int radius, const uint16_t magic[MAX_PIXEL_COUNT]) {
        (void)first_column; // Unused in this function.
        (void)last_column; // Unused in this function.
        (void)magic;

        const uint16_t *srcp = (const uint16_t *)srcp8;
        uint16_t *dstp = (uint16_t *)dstp8;
        stride /= 2;
        stride_w /= 2;

        // Subtract 1 so we can use a less than or equal comparison instead of less than.
        __m128i words_th = _mm_set1_epi16(threshold - 1);
        __m128i words_32768 = _mm_set1_epi16(static_cast<short>(32768u));

        const int pixels_in_xmm = 8;

        // Skip radius pixels on the left and at least radius pixels on the right.
        int width_simd = (width - radius * 2) / pixels_in_xmm * pixels_in_xmm;

        for (int y = 0; y < height; y++) {
            for (int x = radius; x < radius + width_simd; x += pixels_in_xmm) {
                __m128i center_pixel = _mm_loadu_si128((const __m128i *)&srcp[x]);

                __m128i center_lo = _mm_unpacklo_epi16(center_pixel, zeroes);
                __m128i center_hi = _mm_unpackhi_epi16(center_pixel, zeroes);

                __m128i sum_lo = _mm_slli_epi32(center_lo, 1);
                __m128i sum_hi = _mm_slli_epi32(center_hi, 1);

                __m128i counter = _mm_set1_epi16(2);

                for (int yy = std::max(-y, -radius); yy <= std::min(radius, height - y - 1); yy++) {
                    for (int xx = -radius; xx <= radius; xx++) {
                        __m128i neighbour_pixel = _mm_loadu_si128((const __m128i *)&srcp[x + yy * stride + xx]);

                        __m128i abs_diff = _mm_or_si128(_mm_subs_epu16(center_pixel, neighbour_pixel),
                                                        _mm_subs_epu16(neighbour_pixel, center_pixel));

                        // Absolute difference less than or equal to th - 1 will be all zeroes.
                        abs_diff = _mm_subs_epu16(abs_diff, words_th);

                        // 0 words become 65535, not 0 words become 0.
                        __m128i mask = _mm_cmpeq_epi16(abs_diff, zeroes);

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

                __m128 resultf_lo = _mm_mul_ps(_mm_cvtepi32_ps(sum_lo),
                                               _mm_rcp_ps(counter_lo));
                __m128 resultf_hi = _mm_mul_ps(_mm_cvtepi32_ps(sum_hi),
                                               _mm_rcp_ps(counter_hi));

                // Add 0.5f for rounding, subtract 32768 for packssdw.
                resultf_lo = _mm_sub_ps(resultf_lo, _mm_set1_ps(32767.5f));
                resultf_hi = _mm_sub_ps(resultf_hi, _mm_set1_ps(32767.5f));

                __m128i result_lo = _mm_cvttps_epi32(resultf_lo);
                __m128i result_hi = _mm_cvttps_epi32(resultf_hi);

                __m128i result = _mm_add_epi16(_mm_packs_epi32(result_lo, result_hi),
                                               words_32768);

                _mm_storeu_si128((__m128i *)&dstp[x], result);
            }

            srcp += stride;
            dstp += stride_w;
        }

        process_plane_scalar<uint16_t>(srcp8,
                                       dstp8,
                                       0,
                                       radius,
                                       width,
                                       height,
                                       stride * 2,
                                       stride_w * 2,
                                       threshold,
                                       radius,
                                       magic);

        process_plane_scalar<uint16_t>(srcp8,
                                       dstp8,
                                       radius + width_simd,
                                       width,
                                       width,
                                       height,
                                       stride * 2,
                                       stride_w * 2,
                                       threshold,
                                       radius,
                                       magic);
    }

#endif // MINIDEEN_X86
};
