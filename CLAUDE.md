# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

KevqDMA — a Counter-Strike 2 external read-only DMA cheat. The process never touches `cs2.exe`'s memory directly: it talks to a separate machine over PCILeech / FPGA via the bundled `vmm.dll` / `leechcore.dll` (`src/vendor/DMALibrary/`), reads game state, and renders an ImGui + D3D11 transparent overlay on top of the CS2 window. There is also an embedded HTTP / WebSocket "WebRadar" server (default port `22006`) that streams a snapshot JSON to a browser.

## Build

Windows-only. MSBuild project (`KevqDMA.vcxproj`, MSVC `v145` toolset, C++20). The shipping configuration is **Release|x64** (warnings-as-errors `/W4`); other configurations exist but are not actively used.

```bash
# from repo root
msbuild KevqDMA.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Output: `x64/Release/KevqDMA.exe` (alongside `vmm.dll`, `leechcore.dll`, `FTD3XX.dll`, and a `data/` runtime tree).

There is **no test suite** and no lint config. There is no CI — offsets are fetched directly from `a2x/cs2-dumper` at launch (see Architecture below).

Smoke-test the binary without launching the full overlay:

```bash
x64/Release/KevqDMA.exe --offsets-self-test   # exits after offset sync + load
x64/Release/KevqDMA.exe --verbose             # full run with verbose DMA logs
```

## Runtime layout

`KevqDMA.exe` resolves paths via `app::paths::*` (`src/app/Config/project_paths.cpp`). When run from `x64/Release`, everything lives under a sibling `data/` directory:

- `data/offsets/offsets.json` — single source of truth for runtime offsets, regenerated at launch from `a2x/cs2-dumper` (see `runtime_offsets::AutoUpdateFromGitHub`). Legacy `offsets.ini` files are auto-deleted.
- `data/config/<profile>.json` — saved menu profiles (default `KevqDefault`).
- `data/settings/imgui.ini` — ImGui window state (legacy `[Window][KevqDMA ...]` entries are pruned on launch).
- `assets/webradar/` or `src/Features/WebRadar/Assets/` — files served by the WebRadar HTTP server. The candidate list is in `ResolveWebRadarAssetDirectory()`; running directly from the repo source layout works without copying.

`FindProjectRoot()` walks up to 8 parents looking for `KevqDMA.vcxproj` (or `src/`+`include/`+`src/assets/`), so dev runs out of `x64/Release` find the source-tree assets automatically.

## Architecture

The codebase is split into four top-level namespaces / source roots, each mirrored under `include/` and `src/`:

- **`src/app/Bootstrap`** — `main.cpp` orchestrates the startup sequence: console banner → GitHub version check → `runtime_offsets::AutoUpdateFromGitHub` → `runtime_offsets::Load` → `mem.InitDma` (DMALibrary, with hard-coded `VMMDLL_OPT_CONFIG_*` cache tunings) → wait-for-`cs2.exe` loop with periodic `mem.CloseDma()`/re-init → resolve `client.dll` + `engine2.dll` bases into `g::clientBase`/`g::engine2Base` → `overlay::Create` → `esp::StartDataWorker` → `webradar::Initialize` → `overlay::Run`. Press `END` during the cs2-wait loop to abort.
- **`src/app/Platform/overlay.cpp`** — D3D11 + ImGui transparent topmost layered window. Continuously re-parents itself to the cs2 client window (`SyncOverlayBounds`), supports tearing / waitable swap chains, and pushes per-stage timings into `overlay::PerfStats`. Big bodies are split out into `overlay_parts/overlay_create_body.inl` / `overlay_run_body.inl` and `#include`d inside the function definitions — this is intentional, not generated; edit the `.inl` directly.
- **`src/Game/Offsets/runtime_offsets.cpp`** — at every launch, scans open PRs of `a2x/cs2-dumper` via the GitHub API (unauth, 60/hr/IP limit), fetches `output/info.json` from each candidate ref + `main`, picks the newest by timestamp, and if its timestamp differs from the cached `selectedSourceTimestamp` downloads `output/{client_dll,offsets}.hpp` from that ref into a temp dir, parses the C++ `constexpr std::ptrdiff_t` constants with a regex + brace-tracking namespace scanner, and writes `data/offsets/offsets.json`. Cross-references against `steam.inf` from `SteamTracking/GameTracking-CS2` to flag dumps that lag the live patch. On any HTTP failure the cached values are reused. Required vs optional remote fields live in `runtime_offsets_parts/runtime_offsets_remote_fields.inl`. `runtime_offsets::Get()` returns the cached `Values` struct used everywhere.
- **`src/Features/`** — feature modules:
  - `ESP/` — by far the largest. `esp.cpp` is a thin shell: it `#include`s ~40 `.inl` units across `Core/`, `DataReader/`, `Helpers/`, `Render/`, and `Worker/`. Two background threads run continuously: a **data worker** at `DATA_WORKER_HZ = 300` (`Worker/data_worker_loop.inl`) reads entity / bone / world / bomb data via DMA scatter reads, and a **camera worker** at `CAMERA_WORKER_HZ = 500` (`Worker/camera_worker_loop.inl`) reads view matrix / view angles **and** scatters `m_bSpottedByMask` per pawn each tick to derive per-player visibility. Both publish to a triple-buffered snapshot consumed by `esp::Draw` on the render thread and by `esp::GetWebRadarSnapshot`. DMA failure handling and recovery requests live in `Worker/dma_health_helpers.inl` / `try_recover_dma.inl`. **All core ESP logic is in the `.inl` files; `esp.cpp` itself contains almost no code.**
  - `Radar/` — minimap overlay. Per-map calibration profiles in `Calibration/*.inl` are keyed by `C_CSGameRules.m_vMinimap{Mins,Maxs}` bounds and persisted as INI under `data/settings/radar_maps.ini` (see `get_radar_profiles_path.inl`).
  - `WebRadar/` — embedded HTTP+WebSocket server (`Runtime/webradar.cpp`). Pulls snapshots from `esp::GetWebRadarSnapshot`, serves `index.html`/`viewer.{js,css}` from the resolved asset directory, and broadcasts live JSON to connected WS clients. Static assets are also embedded via `embedded_assets.cpp` as a fallback when the on-disk directory is missing.
  - `Settings/` — UI tab only.
- **`src/app/UI/MenuShell`** — generic ImGui menu shell. `MenuController` owns a `vector<unique_ptr<ITabPage>>` populated with the four feature tabs. Default toggle key is `P` (`UiSettings::menuToggleKey`).

### Global state

`include/app/Core/app_state.h` defines the entire mutable app state as plain structs (`DisplaySettings`, `RuntimeState`, `EspSettings`, `RadarSettings`, `WebRadarSettings`, `UiSettings`, `FontState`) inside a single `app::state::globalState` instance. `include/app/Core/globals.h` then exposes hundreds of `inline` references in namespace `g::` (e.g. `g::espEnabled`, `g::clientBase`, `g::screenWidth`). New settings should be added as a field on the matching struct in `app_state.h` AND a reference in `globals.h`, then wired into `config_parts/config_apply_json_body.inl` + `config_save_json_body.inl` for persistence.

### The `.inl` split pattern

Several large translation units (`esp.cpp`, `runtime_offsets.cpp`, `overlay.cpp`, `config.cpp`) are kept short by `#include`-ing implementation bodies from sibling `*_parts/*.inl` files inside their function definitions. The `.inl` files are NOT headers — they are function bodies and include-listed in `KevqDMA.vcxproj` only as `ClInclude`. When editing logic, you almost always want the `.inl`, not the `.cpp` shell.

### DMA access

`src/vendor/DMALibrary/Memory/Memory.cpp` wraps `vmm.dll`. Code accesses the global `mem` instance: `mem.AttachToProcess("cs2.exe", true)`, `mem.GetBaseDaddy("client.dll")`, scatter reads via `mem.AddScatterReadRequest` / `mem.ExecuteReadScatter`. Cache tuning (`VMMDLL_OPT_CONFIG_TICK_PERIOD` etc.) is set in `main.cpp` after each successful `InitDma`; if you re-init DMA you must re-apply those settings.

### Live visibility pipeline

Visibility (the spotted-by-mask flag CS2 uses for the radar) is read by both workers. The data worker reads it at `kPlayerVisibilityAuxUs = 6000` (~166 Hz) and writes `PlayerData::visible` for consumers like the WebRadar snapshot and offscreen arrows. The camera worker reads it at 500 Hz, derives visibility against the local player's mask bits (which the data worker publishes into atomics in `state.inl`: `s_livePawnPointers[64]`, `s_liveLocalMaskBit`, etc.), and writes resolved values back into `s_liveVisible[64]` + `s_liveVisibilityUpdatedUs[64]`. `draw.inl` checks the live atomic first and falls back to `p.visible` when the timestamp is older than `kLiveVisibilityFreshnessUs = 50ms`. Net reaction time is ~2–3 ms — bounded by camera-worker tick + scatter latency, well below CS2's server tick (~7.8 ms at 128 Hz). Going faster requires raycasting against world geometry.

### Vendored dependencies

`src/vendor/` is fully vendored — no package manager. `imgui` (with `dx11`+`win32` backends), `nlohmann/json` (single-header at `vendor/json/json.hpp`), `qrcodegen`, and `DMALibrary` (which wraps PCILeech `vmm.lib`+`leechcore.lib`). Do not introduce a package manager; just drop new sources under `src/vendor/` and add them to `KevqDMA.vcxproj`.

## Versioning

`include/app/Core/build_info.h` hard-codes `kVersionTag` (currently `v1.0.2`) and the GitHub repo URL used by the in-app updater (`bootstrap::CheckForVersionUpdate`). Bump `kVersionTag` when cutting a release; the running binary fetches the same header from `main` over HTTPS to detect updates.
