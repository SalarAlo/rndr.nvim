# rndr.nvim

`rndr.nvim` is a Neovim plugin backed by a native renderer that previews images and simple 3D assets directly inside the current buffer.

This is a compiled plugin. The Lua part runs inside Neovim, but the renderer is a C++ binary that users build during installation.

Supported inputs:

- Raster images: `png`, `jpg`, `jpeg`, `gif`, `bmp`, `webp`, `tga`, `psd`, `hdr`, `pic`, `pnm`, `ppm`, `pgm`, `pbm`
- Vector images: `svg`, `svgz` via `rsvg-convert`, `magick`, or `convert`
- Models: `obj`, `fbx`, `glb`, `gltf`, `dae`, `3ds`, `blend`, `ply`, `stl`, `x`, `off`

## Layout

```text
.
├── lua/rndr/             # Neovim plugin code
├── lua/examples/         # Sample assets
├── scripts/              # Build helpers for plugin managers and local installs
└── renderer/             # Native renderer built with CMake
    ├── src/
    └── vendor/
```

## Requirements

Required:

- Neovim with Lua support
- `git`
- CMake 3.16+
- A C++23-capable compiler

Optional:

- `rsvg-convert`, `magick`, or `convert` for SVG support

You do not need to install `assimp` manually. The renderer build uses a system copy when available and otherwise fetches and builds a bundled copy automatically.

## Install

Clone the repo:

```bash
git clone https://github.com/SalarAlo/rndr_nvim.git
cd rndr_nvim
```

Build the native renderer:

```bash
make
```

That produces the binary at:

```text
renderer/build/rndr
```

`make` is a thin wrapper around `./scripts/build_renderer.sh`.

The build script is intended to work on Linux and macOS. It configures CMake in `Release` mode by default and builds the renderer in the location the plugin already expects.

## Plugin Manager Setup

For plugin managers, make the build step run after clone/update.

`lazy.nvim`:

```lua
{
  "SalarAlo/rndr_nvim",
  build = "make",
  config = function()
    require("rndr").setup()
  end,
}
```

`packer.nvim`:

```lua
use({
  "SalarAlo/rndr_nvim",
  run = "make",
  config = function()
    require("rndr").setup()
  end,
})
```

If you want to keep the renderer somewhere else, override `renderer.bin` in `setup()`.

If a user does not have `make`, they can use the script directly:

```lua
{
  "SalarAlo/rndr_nvim",
  build = "./scripts/build_renderer.sh",
}
```

## Manual Build

If you prefer raw CMake commands, they are:

```bash
cmake -S renderer -B renderer/build -DCMAKE_BUILD_TYPE=Release
cmake --build renderer/build --parallel --config Release
```

## Neovim Setup

Minimal setup:

```lua
require("rndr").setup({
  preview = {
    auto_open = false,
  },
})
```

Check whether the binary and optional SVG tools are available:

```vim
:checkhealth rndr
```

## Usage

Open an image or model file in Neovim, then run:

```vim
:RndrOpen
```

Or render a specific file directly:

```vim
:RndrOpen lua/examples/dog.jpg
```

Commands:

- `:RndrOpen [path]`
- `:RndrClose`
- `:RndrRotateLeft`
- `:RndrRotateRight`
- `:RndrRotateUp`
- `:RndrRotateDown`
- `:RndrResetView`

## Configuration

```lua
require("rndr").setup({
  preview = {
    auto_open = false,
    events = { "BufReadPost" },
    render_on_resize = true,
  },
  assets = {
    images = { "png", "jpg", "jpeg", "gif", "bmp", "webp" },
    vectors = { "svg", "svgz" },
    models = { "obj", "fbx", "glb", "gltf", "dae", "blend", "ply", "stl" },
  },
  window = {
    termguicolors = true,
    size = {
      width_offset = 0,
      height_offset = 0,
      min_width = 1,
      min_height = 1,
    },
    options = {
      number = false,
      relativenumber = false,
      wrap = false,
      signcolumn = "no",
    },
  },
  renderer = {
    bin = "/absolute/path/to/rndr.nvim/renderer/build/rndr",
    supersample = 2,
    brightness = 1.0,
    saturation = 1.18,
    contrast = 1.08,
    gamma = 0.92,
    background = "0d0f14",
  },
  controls = {
    rotate_step = 15,
    keymaps = {
      close = "q",
      rerender = "R",
      reset_view = "0",
      rotate_left = "h",
      rotate_right = "l",
      rotate_up = "k",
      rotate_down = "j",
    },
  },
})
```

Notes:

- The cleaner public shape is `preview`, `assets`, `window`, `renderer`, and `controls`.
- Existing flat keys such as `auto_open`, `renderer_bin`, `render`, `size`, and `win_options` still work for backward compatibility.
- Inspect the default config with `require("rndr").defaults()`.
- By default, `renderer.bin` resolves to `<plugin-root>/renderer/build/rndr`.
- First install can take longer if CMake needs to fetch and build `assimp`.
- `preview.auto_open = true` registers autocommands for `preview.events`.
- The preview is rendered in-place inside the current buffer.
- Use `:RndrClose` to restore the original buffer contents after previewing.
- `preview.render_on_resize = true` rerenders active previews when Neovim windows resize.
- SVG files are rasterized into Neovim's cache directory before rendering.
- Rotation commands only apply to model files.

## Publishing Notes

For a compiled Neovim plugin, the usual publish flow is:

1. Push this repo to GitHub.
2. Keep the install command in the README and plugin manager examples pointed at `make`.
3. Tag releases such as `v0.1.0` so users can pin versions.
4. Make sure `renderer/build/` stays out of the repo.

That is enough for `lazy.nvim`, `packer.nvim`, and similar plugin managers to treat the repo as installable.

## Manual Renderer Usage

The renderer can also be called directly:

```bash
./renderer/build/rndr <file> <term-width> <term-height> [supersample] [yaw] [pitch] [brightness] [saturation] [contrast] [gamma] [background]
```

Example:

```bash
./renderer/build/rndr lua/examples/dog.jpg 100 40
```

The plugin uses `--stdio` mode internally.
