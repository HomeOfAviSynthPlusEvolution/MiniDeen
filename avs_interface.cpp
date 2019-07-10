#include <inttypes.h>
#include "wrapper/avs_filter.hpp"
#include "version.hpp"
#include "minideen_engine.hpp"

class MiniDeenFilter: public AVSFilter {

protected:
  int radius;
  int thrY, thrUV;
  int y, u, v;
  int bit_per_channel;
  uint16_t magic[MAX_PIXEL_COUNT];
  decltype (MiniDeenEngine::process_plane_scalar<uint8_t>) *process_plane;

public:
  virtual const char* name() const { return "MiniDeen"; }
  virtual void initialize() {
    radius = ArgAsInt( 1, "radius", 1);
    thrY   = ArgAsInt( 2, "thrY",   10);
    thrUV  = ArgAsInt( 3, "thrUV",  12);
    y      = ArgAsInt( 4, "y", 3);
    u      = ArgAsInt( 5, "u", 3);
    v      = ArgAsInt( 6, "v", 3);
    bit_per_channel = vi.BitsPerComponent();
    byte_per_channel = bit_per_channel > 8 ? 2 : 1;

    // Check input
    if (!vi.HasVideo())
      throw("where's the video?");
    if (thrY < 2 || thrY > 255)
      throw("threshold (Y) must be between 2 and 255 (inclusive).");
    if (thrUV < 2 || thrUV > 255)
      throw("threshold (UV) must be between 2 and 255 (inclusive).");
    if (!supported_pixel())
      throw("only 8..16 bit integer clips with constant format are supported.");

    int pixel_max = (1 << bit_per_channel) - 1;

    thrY = thrY * pixel_max / 255;
    thrUV = thrUV * pixel_max / 255;
    memset(&magic, 0, sizeof(magic));

    process_plane = (bit_per_channel == 8) ? MiniDeenEngine::process_plane_scalar<uint8_t>
                                           : MiniDeenEngine::process_plane_scalar<uint16_t>;
#if defined (MINIDEEN_X86)
    process_plane = (bit_per_channel == 8) ? MiniDeenEngine::process_plane_sse2_8bit
                                           : MiniDeenEngine::process_plane_sse2_16bit;

    for (int i = 2; i < MAX_PIXEL_COUNT; i++)
        magic[i] = (unsigned)(65536.0 / i + 0.5);
#endif

  }

  virtual AVSFilter::AFrame get(int n) {
    auto src = GetFrame(child, n);
    auto dst = NewFrame();
    auto widthY = width(src, PLANAR_Y);
    auto widthUV = width(src, PLANAR_U);
    auto heightY = height(src, PLANAR_Y);
    auto heightUV = height(src, PLANAR_U);
    auto strideY = stride(src, PLANAR_Y);
    auto strideUV = stride(src, PLANAR_U);
    if (y == 3)
      process_plane(src->GetReadPtr(PLANAR_Y), dst->GetWritePtr(PLANAR_Y), 0, widthY, widthY, heightY, strideY, thrY, radius, magic);
    else if (y == 2)
      _env->BitBlt(dst->GetWritePtr(PLANAR_Y), strideY, src->GetReadPtr(PLANAR_Y), strideY, widthY * byte_per_channel, heightY);
    if (u == 3)
      process_plane(src->GetReadPtr(PLANAR_U), dst->GetWritePtr(PLANAR_U), 0, widthUV, widthUV, heightUV, strideUV, thrUV, radius, magic);
    else if (u == 2)
      _env->BitBlt(dst->GetWritePtr(PLANAR_U), strideUV, src->GetReadPtr(PLANAR_U), strideUV, widthUV * byte_per_channel, heightUV);
    if (v == 3)
      process_plane(src->GetReadPtr(PLANAR_V), dst->GetWritePtr(PLANAR_V), 0, widthUV, widthUV, heightUV, strideUV, thrUV, radius, magic);
    else if (v == 2)
      _env->BitBlt(dst->GetWritePtr(PLANAR_V), strideUV, src->GetReadPtr(PLANAR_V), strideUV, widthUV * byte_per_channel, heightUV);

    return dst;
  }
public:
  using AVSFilter::AVSFilter;
};


const AVS_Linkage *AVS_linkage = NULL;

AVSValue __cdecl CreateAVSFilter(AVSValue args, void* user_data, IScriptEnvironment* env)
{
  auto filter = new MiniDeenFilter(args, env);
  try {
    filter->initialize();
  }
  catch (const char *err) {
    env->ThrowError("%s %s: %s", filter->name(), PLUGIN_VERSION, err);
  }
  return filter;
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, AVS_Linkage* vectors)
{
  AVS_linkage = vectors;
  env->AddFunction("MiniDeen", "c[radius]i[thrY]i[thrUV]i[y]i[u]i[v]i", CreateAVSFilter, 0);
  return "MiniDeen";
}
