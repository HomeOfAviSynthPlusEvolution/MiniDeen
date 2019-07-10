# MiniDeen

MiniDeen Copyright(C) 2019 Xinyue Lu

MiniDeen is a spatial denoising filter. It replaces every pixel with the average of its neighbourhood.

This is a dual interface port of the [VapourSynth plugin MiniDeen](https://github.com/dubhater/vapoursynth-minideen) version beta 2.


## Usage

```
LoadPlugin("MiniDeen.dll")
MiniDeen(clip, radius=1, thrY=10, thrUV=12, Y=3, U=3, V=3)
```

Parameters:

- *clip*

    A clip to process. It must have constant format and it must be 8..16 bit with integer samples.

- *radius*

    Size of the neighbourhood. Must be between 1 (3x3) and 7 (15x15).

    Default: 1.

- *thrY*, *thrUV*

    Only pixels that differ from the center pixel by less than the *threshold* will be included in the average. Must be between 2 and 255.

    The threshold is scaled internally according to the bit depth.

    Smaller values will filter more conservatively.

    Default: 10 for the Y plane, and 12 for the other planes.

- *y*, *u*, *v*

    Whether a plane is to be filtered.

        1 - Do not touch, leaving garbage data
        2 - Copy from origin
        3 - Process

    Default: 3.


## Compilation (MSVC)

```cmd
mkdir build\x86
pushd build\x86
cmake -DCMAKE_GENERATOR_PLATFORM=Win32 -D_DIR=x86 ..\..\
popd
mkdir build\x64
pushd build\x64
cmake -DCMAKE_GENERATOR_PLATFORM=x64 -D_DIR=x64 ..\..\
popd
cmake --build build\x86 --config Release
cmake --build build\x64 --config Release
```


## License

* ISC for VapourSynth interface and engine.

* MIT for AviSynth+ interface.
