# furnace-presenter

Video renderer for Furnace modules.

It loads a `.fur` module, renders audio with Furnace itself, draws per-channel oscilloscope + piano-roll visuals, and encodes an MP4 with ffmpeg.

## What it does

- uses Furnace engine playback/render state directly
- renders 60fps video with synced audio
- supports per-channel oscilloscope + piano roll
- supports GUI preview + CLI rendering
- supports `--hide-unused`
- supports custom output resolution with `--width` and `--height`
- supports `--fadeout-ms` for time-based fadeouts

## Requirements

- CMake
- a C++17 compiler
- ffmpeg in `PATH`
- git submodules

## Build

```sh
git submodule update --init --recursive
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_MAKE_PROGRAM=mingw32-make
make furnace-presenter -j4
```

## CLI

Basic render:

```sh
furnace-presenter.exe --input song.fur --output song.mp4
```

Hide unused channels:

```sh
furnace-presenter.exe --input song.fur --output song.mp4 --hide-unused
```

1440p render:

```sh
furnace-presenter.exe --input song.fur --output song.mp4 --width 2560 --height 1440 --hide-unused
```

Two loops with a 10 second fadeout:

```sh
furnace-presenter.exe --input song.fur --output song.mp4 --stop loops:2 --fadeout-ms 10000
```