# CyberpunkWallpaper

Animated cyberpunk/matrix-style live desktop wallpaper in C++ + OpenGL.

## Features
- 3D perspective: character streams distributed across Z depth, closer = larger/faster/brighter
- Katakana + latin + symbol glyphs rendered via FreeType into a GPU texture atlas
- Instanced rendering — thousands of glyphs in a single draw call
- Post-processing pass: glitch strip corruption, chromatic aberration, scanlines, vignette, film grain
- Neon green / cyan / hot pink color palette with depth-based dimming
- Optional Win32 desktop pinning (runs behind icons, no taskbar)

---

## Dependency Setup

Create a `deps/` folder alongside the project root:

```
CyberpunkWallpaper/    <- this folder (contains .vcxproj)
deps/
  glfw/
    include/GLFW/...
    lib-vc2022/glfw3.lib
  glad/
    include/glad/glad.h
    include/KHR/khrplatform.h
    src/glad.c
  glm/
    glm/glm.hpp  (header-only)
  freetype/
    include/ft2build.h
    include/freetype/...
    lib/freetype.lib
```

### Getting each dependency

**GLFW** — https://www.glfw.org/download.html
- Download the 64-bit Windows pre-compiled binaries
- Copy `include/` and `lib-vc2022/` into `deps/glfw/`

**GLAD** — https://glad.dav1d.de/
- Language: C/C++, Specification: OpenGL, Profile: Core, Version: 3.3
- Add `gl` extension (no extras needed)
- Generate and download, copy `include/` and `src/` into `deps/glad/`

**GLM** — https://github.com/g-truc/glm/releases
- Header-only; just copy the `glm/` folder into `deps/glm/`

**FreeType** — https://github.com/ubawurinna/freetype-windows-binaries
- Copy `include/` and `release dll/win64/freetype.lib` into `deps/freetype/`
- Copy `freetype.dll` next to your built .exe

---

## Building

1. Open `CyberpunkWallpaper.vcxproj` in Visual Studio 2022
2. Verify the include/lib paths in the project properties match your `deps/` layout
3. Select a build configuration and build:

| Configuration        | Output               | Use for                          |
|----------------------|----------------------|----------------------------------|
| Debug                | .exe (console)       | Development / wallpaper testing  |
| Release              | .exe (no console)    | Silent wallpaper mode            |
| Screensaver Debug    | .scr                 | Screensaver testing              |
| Screensaver Release  | .scr                 | Final screensaver install        |

The shaders are loaded at runtime from the `shaders/` folder.
Make sure the working directory (Debug settings → Working Directory) is set to `$(ProjectDir)`.

---

## Installing as a Screensaver

1. Build the **Screensaver Release** configuration — produces `CyberpunkWallpaper.scr`
2. Copy `CyberpunkWallpaper.scr` **and** the `shaders/` folder to `C:\Windows\System32\`
   - The screensaver runs from System32, so shaders must live there too
   - Alternatively, hardcode the shader path in `ShaderUtils::readFile()` if you prefer a fixed location
3. Copy `freetype.dll` (from `deps/freetype/`) to `C:\Windows\System32\` as well
4. Right-click `CyberpunkWallpaper.scr` → **Install**, or go to:
   `Settings → Personalization → Lock screen → Screen saver settings`
   and select **CyberpunkWallpaper** from the dropdown
5. Click **Preview** to test — Windows passes `/p <hwnd>` and you'll see it in the tiny pane

### Screensaver command-line modes (Windows calls these automatically)
| Arg        | Behavior                                      |
|------------|-----------------------------------------------|
| `/s`       | Fullscreen screensaver — exits on any input   |
| `/p <hwnd>`| Renders into the Settings preview pane        |
| `/c`       | Shows a simple config info dialog             |
| *(none)*   | Normal windowed mode (dev/wallpaper)          |

---

## Configuration (main.cpp top)

| Option             | Default | Description                                    |
|--------------------|---------|------------------------------------------------|
| `SCREEN_W/H`       | 1920×1080 | Resolution                                   |
| `FULLSCREEN`       | false   | Exclusive fullscreen mode                      |
| `PIN_TO_DESKTOP`   | false   | Parent window behind desktop icons (wallpaper) |
| `TARGET_FPS`       | 60      | Frame rate cap                                 |

Set `PIN_TO_DESKTOP = true` and `FULLSCREEN = false` for live wallpaper mode.

---

## Architecture

```
main.cpp           — init, main loop, timing
GlyphAtlas.h       — FreeType → single GL texture atlas
ColumnManager.h    — 3D column simulation, glitch state, instance data
Renderer.h         — instanced glyph draw (VAO/VBO, projection)
PostProcessor.h    — FBO + full-screen post pass
ShaderUtils.h      — shader compile/link helpers
Wallpaper.h        — Win32 WorkerW desktop pinning
shaders/
  glyph.vert/frag  — per-instance glyph quad, shimmer effect
  post.vert/frag   — glitch strips, chroma shift, scanlines, vignette
```

---

## Customization Ideas

- **Colors**: edit `ColumnManager.h` COLOR_HEAD / COLOR_MID / COLOR_CYAN / COLOR_PINK
- **More glitch**: increase `glitchIntensity` floor or lower `glitchSpawnTimer` range
- **Fog layers**: add a `uFogColor` uniform to `glyph.frag` and lerp by Z
- **City skyline**: add a second render pass with a geometry-shader generated silhouette
- **Audio reactivity**: sample Windows WASAPI loopback audio and drive `glitchIntensity`
