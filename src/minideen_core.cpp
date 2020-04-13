#include "minideen_common.h"
#include <algorithm>

template <typename PixelType>
void minideen_C(const uint8_t *srcp8, uint8_t *dstp8, int width, int height, int src_stride, int dst_stride, unsigned threshold, int radius) {
  const PixelType *srcp = (const PixelType *)srcp8;
  PixelType *dstp = (PixelType *)dstp8;
  src_stride /= sizeof(PixelType);
  dst_stride /= sizeof(PixelType);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      unsigned center_pixel = srcp[x];

      unsigned sum = center_pixel * 2;
      unsigned counter = 2;

      for (int yy = std::max(-y, -radius); yy <= std::min(radius, height - y - 1); yy++) {
        for (int xx = std::max(-x, -radius); xx <= std::min(radius, width - x - 1); xx++) {
          unsigned neighbour_pixel = srcp[x + yy * src_stride + xx];

          if (threshold > (unsigned)std::abs((int)center_pixel - (int)neighbour_pixel)) {
            counter++;
            sum += neighbour_pixel;
          }
        }
      }

      dstp[x] = (sum * 2 + counter) / (counter * 2);
    }

    srcp += src_stride;
    dstp += dst_stride;
  }
}

template void minideen_C<uint8_t>(const uint8_t *, uint8_t *, int, int, int, int, unsigned, int);
template void minideen_C<uint16_t>(const uint8_t *, uint8_t *, int, int, int, int, unsigned, int);
