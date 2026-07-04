# Soft View

Lightweight, 100% native, no sandbox, supports both X11 and Wayland.
Watch videos, view images, and listen to music using a single engine (libmpv).

## Architecture

* **SDL2** — window + input, auto-detects X11/Wayland
* **libmpv** (render API) — decode/render video, images, and audio
* **Dear ImGui** — lightweight overlay control bar (no widget tree to initialize)

## Dependencies

### Debian / Ubuntu

```bash
sudo apt install build-essential cmake pkg-config libsdl2-dev libmpv-dev

```

### Fedora

```bash
sudo dnf install gcc-c++ cmake pkgconf-pkg-config SDL2-devel mpv-libs-devel

```

### Arch

```bash
sudo pacman -S base-devel cmake sdl2 mpv

```

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

```

This yields a binary at `build/soft-view` — you can run it directly without any additional installation.

## Usage

```bash
./soft-view /path/to/video.mp4
./soft-view /path/to/photo.jpg
./soft-view /path/to/song.mp3

```

Alternatively, open the application without arguments and drag-and-drop a file into the window.

## Shortcuts

| Key | Action |
| --- | --- |
| Space | Play / Pause |
| F | Fullscreen |
| Esc | Exit fullscreen |
| ← / → | Seek backward/forward 5 seconds |
| Mouse movement | Show control bar (auto-hides after 2.5s during playback) |