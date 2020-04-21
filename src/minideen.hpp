/*
 * Copyright 2020 Xinyue Lu
 *
 * Temporal Median - filter.
 *
 */

#pragma once

#include "minideen_common.h"

int GetCPUFlags();

// static constexpr int max_radius {15};
// static constexpr int pixel_count {max_radius * max_radius + 2 + 1};

struct MiniDeen : Filter {
  int process[4] {2, 2, 2, 2};
  int threshold[3] {10, 12, 12};
  int radius[3] {1, 1, 1};
  int opt {0};
  InDelegator* _in;
  bool bypass {true};
  // uint16_t rcp[pixel_count] {0};

  void (*minideen_core)(const uint8_t *, uint8_t *, int, int, int, int, unsigned, int);

  const char* VSName() const override { return "MiniDeen"; }
  const char* AVSName() const override { return "neo_minideen"; }
  const MtMode AVSMode() const override { return MT_NICE_FILTER; }
  const VSFilterMode VSMode() const override { return fmParallel; }
  const std::vector<Param> Params() const override {
    return std::vector<Param> {
      Param {"clip", Clip, false, true, true, false},
      Param {"radius", Integer, true, false, true},
      Param {"threshold", Integer, true, false, true},
      Param {"planes", Integer, true, false, true},
      Param {"radius", Integer, false, true, false},
      Param {"radiusY", Integer, false, true, false},
      Param {"radiusUV", Integer, false, true, false},
      Param {"thrY", Integer, false, true, false},
      Param {"thrUV", Integer, false, true, false},
      Param {"y", Integer, false, true, false},
      Param {"u", Integer, false, true, false},
      Param {"v", Integer, false, true, false},
      Param {"opt", Integer}
    };
  }
  void Initialize(InDelegator* in, DSVideoInfo in_vi, FetchFrameFunctor* fetch_frame) override
  {
    Filter::Initialize(in, in_vi, fetch_frame);

    try {
      std::vector<int> user_planes {0, 1, 2};
      std::vector<int> tmp;
      in->Read("planes", user_planes);
      for (auto &&p : user_planes)
      {
        if (p < in_vi.Format.Planes)
          process[p] = 3;
      }
      in->Read("radius", tmp);
      if (tmp.size() > 0)
        for (int i = 0; i < in_vi.Format.Planes; i++)
        {
          if (i < tmp.size())
            radius[i] = tmp[i];
          else
            radius[i] = radius[i-1];
        }
      tmp.clear();
      in->Read("threshold", tmp);
      if (tmp.size() > 0)
        for (int i = 0; i < in_vi.Format.Planes; i++)
        {
          if (i < tmp.size())
            threshold[i] = tmp[i];
          else
            threshold[i] = threshold[i-1];
        }
    }
    catch (const char *) {
      process[0] =
      process[1] =
      process[2] = 3;
      in->Read("y", process[0]);
      in->Read("u", process[1]);
      in->Read("v", process[2]);

      int radius_tmp = -1;
      in->Read("radius", radius_tmp);
      if (radius_tmp > 0)
        radius[0] = radius[1] = radius[2] = radius_tmp;

      radius_tmp = -1;
      in->Read("radiusY", process[0]);
      in->Read("radiusUV", radius_tmp);
      if (radius_tmp > 0)
        radius[1] = radius[2] = radius_tmp;

      int threshold_tmp = -1;
      in->Read("thrY", process[0]);
      in->Read("thrUV", threshold_tmp);
      if (threshold_tmp > 0)
        threshold[1] = threshold[2] = threshold_tmp;
    }
    in->Read("opt", opt);

    if ((threshold[0] < 0 || threshold[0] > 255) && process[0] == 3)
      throw("threshold (Y) must be between 2 and 255 (inclusive).");
    if ((threshold[1] < 0 || threshold[1] > 255) && process[1] == 3)
      throw("threshold (U) must be between 2 and 255 (inclusive).");
    if ((threshold[2] < 0 || threshold[2] > 255) && process[2] == 3)
      throw("threshold (V) must be between 2 and 255 (inclusive).");
    if ((radius[0] < 1 || radius[0] > 7) && process[0] == 3)
      throw("radius (Y) must be between 1 and 7 (inclusive).");
    if ((radius[1] < 1 || radius[1] > 7) && process[1] == 3)
      throw("radius (U) must be between 1 and 7 (inclusive).");
    if ((radius[2] < 1 || radius[2] > 7) && process[2] == 3)
      throw("radius (V) must be between 1 and 7 (inclusive).");
    if (!in_vi.Format.IsInteger)
      throw("only 8..16 bit integer clips with constant format are supported.");
    if (!in_vi.Format.IsFamilyYUV)
      throw("only YUV clips are supported.");

    if (threshold[0] < 2 && process[0] == 3) process[0] = 2;
    if (threshold[1] < 2 && process[1] == 3) process[1] = 2;
    if (threshold[2] < 2 && process[2] == 3) process[2] = 2;

    int pixel_max = (1 << in_vi.Format.BitsPerSample) - 1;

    for (int i = 0; i < in_vi.Format.Planes; i++) {
      if (process[i] == 3)
        bypass = false;
      threshold[i] = threshold[i] * pixel_max / 255;
    }

    // for (int i = 2; i < pixel_count; i++)
        // rcp[i] = (unsigned)(65536.0 / i + 0.5);

    int CPUFlags = GetCPUFlags();
    switch (in_vi.Format.BytesPerSample) {
      case 1: minideen_core = minideen_C<uint8_t>; break;
      case 2: minideen_core = minideen_C<uint16_t>; break;
    }

    if ((CPUFlags & CPUF_SSE2) && (opt <= 0 || opt > 1)) {
      switch (in_vi.Format.BytesPerSample) {
        case 1: minideen_core = minideen_SSE2_8; break;
        case 2: minideen_core = minideen_SSE2_16; break;
      }
    }
    if ((CPUFlags & CPUF_AVX2) && (opt <= 0 || opt > 2)) {
      switch (in_vi.Format.BytesPerSample) {
        case 1: minideen_core = minideen_AVX2_8; break;
        case 2: minideen_core = minideen_AVX2_16; break;
      }
    }
  }

  DSFrame GetFrame(int n, std::unordered_map<int, DSFrame> in_frames) override
  {
    auto src = in_frames[n];
    if (bypass)
      return src;
    auto dst = src.Create(false);

    for (int p = 0; p < in_vi.Format.Planes; p++)
    {
      bool chroma = in_vi.Format.IsFamilyYUV && p > 0 && p < 3;
      auto height = in_vi.Height;
      auto width = in_vi.Width;
      auto src_stride = src.StrideBytes[p];
      auto src_ptr = src.SrcPointers[p];
      auto dst_stride = dst.StrideBytes[p];
      auto dst_ptr = dst.DstPointers[p];

      if (chroma) {
        height >>= in_vi.Format.SSH;
        width >>= in_vi.Format.SSW;
      }

      if (process[p] == 2) {
        framecpy(dst_ptr, dst_stride, src_ptr, src_stride, width * in_vi.Format.BytesPerSample, height);
        continue;
      }
      if (process[p] != 3)
        continue;

      minideen_core(src_ptr, dst_ptr, width, height, src_stride, dst_stride, threshold[p], radius[p]);
    }

    return dst;
  }

  void framecpy(unsigned char * dst_ptr, int dst_stride, const unsigned char * src_ptr, int src_stride, int width_byte, int height) {
    if (src_stride == dst_stride) {
      memcpy(dst_ptr, src_ptr, dst_stride * height);
      return;
    }
    for (int h = 0; h < height; h++)
    {
      memcpy(dst_ptr, src_ptr, width_byte);
      dst_ptr += dst_stride;
      src_ptr += src_stride;
    }
  }

  ~MiniDeen() = default;
};
