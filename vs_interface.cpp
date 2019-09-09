#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <VapourSynth.h>
#include <VSHelper.h>
#include "version.hpp"
#include "minideen_engine.hpp"

typedef struct MiniDeenData {
    VSNodeRef *clip;
    const VSVideoInfo *vi;

    int radius[3];
    int threshold[3];
    int process[3];

    uint16_t magic[MAX_PIXEL_COUNT];

    decltype (MiniDeenEngine::process_plane_scalar<uint8_t>) *process_plane;
} MiniDeenData;


static void VS_CC minideenInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    (void)in;
    (void)out;
    (void)core;

    MiniDeenData *d = (MiniDeenData *) *instanceData;

    vsapi->setVideoInfo(d->vi, 1, node);
}


static const VSFrameRef *VS_CC minideenGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const MiniDeenData *d = (const MiniDeenData *) *instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->clip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->clip, frameCtx);

        const VSFrameRef *plane_src[3] = {
            d->process[0] ? nullptr : src,
            d->process[1] ? nullptr : src,
            d->process[2] ? nullptr : src
        };
        int planes[3] = { 0, 1, 2 };

        VSFrameRef *dst = vsapi->newVideoFrame2(d->vi->format,
                                                vsapi->getFrameWidth(src, 0),
                                                vsapi->getFrameHeight(src, 0),
                                                plane_src,
                                                planes,
                                                src,
                                                core);

        for (int plane = 0; plane < d->vi->format->numPlanes; plane++) {
            if (!d->process[plane])
                continue;

            const uint8_t *srcp = vsapi->getReadPtr(src, plane);
            uint8_t *dstp = vsapi->getWritePtr(dst, plane);
            int stride = vsapi->getStride(src, plane);
            int width = vsapi->getFrameWidth(src, plane);
            int height = vsapi->getFrameHeight(src, plane);
            int strideW = vsapi->getStride(dst, plane);

            d->process_plane(srcp, dstp, 0, width, width, height, stride, strideW, d->threshold[plane], d->radius[plane], d->magic);
        }

        vsapi->freeFrame(src);

        return dst;
    }

    return nullptr;
}


static void VS_CC minideenFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    MiniDeenData *d = (MiniDeenData *)instanceData;

    vsapi->freeNode(d->clip);
    free(d);
}


static void VS_CC minideenCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    MiniDeenData d;
    memset(&d, 0, sizeof(d));

    int err;

    for (int i = 0; i < 3; i++) {
        d.radius[i] = int64ToIntS(vsapi->propGetInt(in, "radius", i, &err));
        if (err)
            d.radius[i] = (i == 0) ? 1
                                   : d.radius[i - 1];

        d.threshold[i] = int64ToIntS(vsapi->propGetInt(in, "threshold", i, &err));
        if (err)
            d.threshold[i] = (i == 0) ? 10
                                      : d.threshold[i - 1];

        if (d.radius[i] < 1 || d.radius[i] > 7) {
            vsapi->setError(out, "MiniDeen: radius must be between 1 and 7 (inclusive).");
            return;
        }

        if (d.threshold[i] < 2 || d.threshold[i] > 255) {
            vsapi->setError(out, "MiniDeen: threshold must be between 2 and 255 (inclusive).");
            return;
        }
    }

    d.clip = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.clip);

    if (!d.vi->format ||
        d.vi->format->bitsPerSample > 16 ||
        d.vi->format->sampleType != stInteger) {
        vsapi->setError(out, "MiniDeen: only 8..16 bit integer clips with constant format are supported.");
        vsapi->freeNode(d.clip);
        return;
    }


    int n = d.vi->format->numPlanes;
    int m = vsapi->propNumElements(in, "planes");

    for (int i = 0; i < 3; i++)
        d.process[i] = (m <= 0);

    for (int i = 0; i < m; i++) {
        int o = int64ToIntS(vsapi->propGetInt(in, "planes", i, 0));

        if (o < 0 || o >= n) {
            vsapi->freeNode(d.clip);
            vsapi->setError(out, "MiniDeen: plane index out of range");
            return;
        }

        if (d.process[o]) {
            vsapi->freeNode(d.clip);
            vsapi->setError(out, "MiniDeen: plane specified twice");
            return;
        }

        d.process[o] = 1;
    }


    int pixel_max = (1 << d.vi->format->bitsPerSample) - 1;

    for (int i = 0; i < 3; i++)
        d.threshold[i] = d.threshold[i] * pixel_max / 255;


    d.process_plane = (d.vi->format->bitsPerSample == 8) ? MiniDeenEngine::process_plane_scalar<uint8_t>
                                                         : MiniDeenEngine::process_plane_scalar<uint16_t>;
#if defined (MINIDEEN_X86)
    d.process_plane = (d.vi->format->bitsPerSample == 8) ? MiniDeenEngine::process_plane_sse2_8bit
                                                         : MiniDeenEngine::process_plane_sse2_16bit;

    for (int i = 2; i < MAX_PIXEL_COUNT; i++)
        d.magic[i] = (unsigned)(65536.0 / i + 0.5);
#endif


    MiniDeenData *data = (MiniDeenData *)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "MiniDeen", minideenInit, minideenGetFrame, minideenFree, fmParallel, 0, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.nodame.minideen", "minideen" PLUGIN_VERSION, "Spatial convolution with thresholds", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("MiniDeen",
                 "clip:clip;"
                 "radius:int[]:opt;"
                 "threshold:int[]:opt;"
                 "planes:int[]:opt;"
                 , minideenCreate, 0, plugin);
}
