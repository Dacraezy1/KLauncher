# KLauncher 🎮

A **fast, modern, native Qt6 C++ Minecraft Java Edition launcher** for Linux.  
Free and open source — licensed under **GPL v3**.

[![Build KLauncher](https://github.com/Dacraezy1/KLauncher/actions/workflows/build.yml/badge.svg)](https://github.com/Dacraezy1/KLauncher/actions/workflows/build.yml)

---

## Features

| Category | Details |
|----------|---------|
| **Instances** | Create unlimited instances, each with their own mods, saves, configs, and resource packs |
| **All MC versions** | Every Minecraft release, snapshot, old beta, and old alpha via Mojang's version manifest |
| **Mod loaders** | Fabric, Forge, NeoForge, and Quilt — pick the loader and version per instance |
| **Online login** | Microsoft/Xbox account authentication via OAuth2 device-code flow (no password stored) |
| **Offline mode** | Play single-player and offline servers with any username |
| **Mod browser** | Search and install mods from **Modrinth** and **CurseForge** directly inside the launcher |
| **Java management** | Auto-detects all system Java installs; downloads Adoptium Temurin JRE for any major version |
| **Performance JVM** | Pre-applied G1GC flags, GC pause tuning, pre-touch heap, and more |
| **5 themes** | Dark (default), Light, Deep Blue, Midnight Green, Nord |
| **Console output** | Live colourised game log with save-to-file |
| **System tray** | Minimize to tray, keep running while the game is active |

---

## Download

Go to [**Releases**](https://github.com/Dacraezy1/KLauncher/releases) and download the latest `.AppImage`.

```bash
chmod +x KLauncher-v*.AppImage
./KLauncher-v*.AppImage
```

No external dependencies required — Qt and everything else is bundled in the AppImage.

---

## Building from source

### Requirements

- CMake ≥ 3.22
- Qt 6.4+ (`qt6-base-dev`, `qt6-webengine-dev`, `qt6-tools-dev`)
- GCC / Clang with C++20 support
- Arch Linux: `sudo pacman -S qt6-base qt6-webengine cmake ninja`
- Ubuntu 22.04+: `sudo apt install qt6-base-dev qt6-webengine-dev cmake ninja-build`

### Build

```bash
git clone https://github.com/Dacraezy1/KLauncher.git
cd KLauncher
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel
./build/KLauncher
```

---

## Project structure

```
KLauncher/
├── main.cpp
├── CMakeLists.txt
├── resources/
│   ├── icons/
│   ├── images/
│   ├── themes/          ← .qss theme files
│   └── resources.qrc
├── src/
│   ├── auth/            ← Microsoft OAuth2 authentication
│   ├── java/            ← Java detection & Adoptium download
│   ├── minecraft/       ← Version manifest, download, game launch
│   ├── modloaders/      ← Fabric / Forge / NeoForge / Quilt install
│   ├── mods/            ← Modrinth & CurseForge API
│   ├── ui/              ← All Qt widgets and pages
│   └── utils/           ← DownloadManager, AppConfig
├── scripts/
│   ├── klauncher.desktop
│   └── gen_icons.py
└── .github/workflows/
    └── build.yml        ← GitHub Actions CI/CD → AppImage release
```

---

## GitHub Actions CI/CD

Every push to `main`/`master` builds the launcher on Ubuntu 22.04.  
Pushing a tag like `v1.0.0` **automatically creates a GitHub Release** with the AppImage and tarball attached.

```bash
# Tag and release
git tag v1.0.0
git push origin v1.0.0
```

---

## License

KLauncher is free software released under the **GNU General Public License v3**.  
See [LICENSE](LICENSE) for the full text.

> This project is not affiliated with Mojang Studios or Microsoft.  
> Minecraft is a trademark of Mojang Studios.
