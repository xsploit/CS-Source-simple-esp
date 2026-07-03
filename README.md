# CS:Source Simple ESP — v93 x64 External

An external ESP for Counter-Strike: Source v93 (x64), built for educational
purposes. Reads game memory via `ReadProcessMemory`, renders on a transparent
DirectX 9 + ImGui overlay. No injection, no DLL, no game file modification.

**Fork of [fereshetyan/CS-Source-simple-esp](https://github.com/fereshetyan/CS-Source-simple-esp)**
— credit to the original author for the base architecture (Core/Game/Overlay
module split, RPM layer, ImGui overlay). This fork adds chams, a config
system, color customization, verified v93 x64 offsets, and a netvar dumper.

---

## Features

### ESP
- **Bone-glow chams** — thick glowing lines along the skeleton + bright core
  lines + filled joint dots. Tracks the player's real pose through walls.
  Per-link validity (partial body renders when partially offscreen).
- **Skeleton** — thin wireframe variant (toggleable alongside chams)
- **Box ESP** — outline / filled / corner styles
- **Head dot** — bone-accurate, range-scaled radius, lifted to visual head center
- **Health bar + HP text** — green→red lerp, configurable position
- **Name ESP** — reads from the player resource table
- **Distance text** — metres or raw Hammer units
- **Snaplines** — faint line from screen-center-bottom to each enemy

### Config
- **Persistent JSON config** — autosaves next to the exe on exit/END
- **7 configurable colors** with alpha — enemy, team, skeleton, head dot,
  name, HP text, snapline (ImGui `ColorEdit4`)
- **ESP presets** — Clean / Competitive / Walls+Chams / Everything
- **Live tuning** — box style, box thickness, chams thickness/core/joint,
  head lift, head dot size, distance fade, stale timeout

### Architecture
- **No-flicker render pipeline** — two-pass: read all entities into
  `vector<RenderRecord>` (all RPM), then draw from records (pure ImGui).
  Prevents half-drawn frames from RPM latency spikes.
- **m_bConnected liveness filter** — the engine's own ground-truth flag for
  "is this slot occupied by a live player." Campers render forever; round-end
  corpses vanish. No heuristic, no tuning, no false positives.
- **Range-scaled rendering** — alpha fades with distance, head dot auto-sizes

---

## How It Works

### External memory reading
Runs as a standalone `.exe`, reads the game via `ReadProcessMemory`. No
injection, no hooks inside the game process, no file modifications.

### Verified offsets (v93 x64)
Every offset below was **empirically verified against a live game** via a
Cheat Engine MCP bridge — not from forum dumps. Full details in `SPEC.md`.

| Field | Offset | Source |
|-------|--------|--------|
| Entity list base | `client.dll + 0x6098C8` | Live probe ✅ |
| Entity stride | `0x20` (ptr at +0x0) | Live probe ✅ |
| `m_iHealth` | `+0xD0` | Live probe ✅ |
| `m_iTeamNum` | `+0xD8` | Live probe ✅ |
| `m_vecOrigin` | `+0x428` | Live probe ✅ |
| `m_dwBoneMatrix` | `+0x810` | In-game render ✅ |
| `dwViewMatrix` | `engine.dll + 0x6A1BD0` | In-game render ✅ |
| `dwPlayerResource` | `client.dll + 0x5FDF98` | Live probe ✅ |
| `m_bConnected` | `player_resource + 0x10D8` | Live probe ✅ |
| `m_szName` | `player_resource + 0x790` | Live probe ✅ |

### Liveness: why not dormant?
`m_bDormant` is **not a networked property** in CS:S v93 x64 — it's an
internal vtable call (`IClientNetworkable::IsDormant()`), unreadable
externally. This was confirmed by live probing AND the StaLLyyyy dump
(`m_bDormant = 0x0 // not a RecvProp`).

Instead, we use `m_bConnected` from the player resource table — a bool array
indexed by player slot. A slot reads `false` when the player disconnects or
their corpse is cleaned up at round end. This is the ground-truth signal the
engine itself uses.

---

## Core Components

### `Core::Memory`
Wraps `OpenProcess` + `ReadProcessMemory` + `CreateToolhelp32Snapshot`.
Template `Read<T>()` reads any type. `GetModuleBase()` finds `client.dll` /
`engine.dll` at runtime.

### `Core::Config`
JSON save/load for all toggles + colors. Resolves the config path relative
to the EXE folder (not CWD). Autosaves on exit.

### `Game::Entity`
Offset-based accessors: `GetHealth()`, `GetTeam()`, `GetPosition()`,
`GetVelocity()`, `GetBonePositions()`.

### `Overlay::Window`
DirectX 9 transparent overlay. `WS_EX_LAYERED | WS_EX_TRANSPARENT` when
playing (click-through), removes `TRANSPARENT` when menu is open (interactive).
Input state synced to `menuOpen` every frame.

### `Utils::Math`
`WorldToScreen()` — standard view-projection transform from the engine's 4x4
view matrix. Returns false if the point is behind the camera.

---

## Build

### Prerequisites
- Visual Studio 2022 (C++ Desktop Development)
- CMake 3.15+

### Steps
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Output: `build/Release/CSS_Educational_ESP.exe`

**Run as Administrator** (required for `OpenProcess` with `PROCESS_ALL_ACCESS`).

---

## Usage

1. Launch CS:S (`cstrike_win64.exe`)
2. Load into a map (local server with bots works)
3. Run `CSS_Educational_ESP.exe` as admin
4. **INSERT** — toggle menu / mouse input
5. **END** — exit (autosaves config)

### Config tab
- Save / Reload buttons
- Config path shown next to the buttons

### Style tab
- Box style (outline / filled / corner) + thickness
- Chams thickness / core / joint radius
- Head lift / head dot size
- Distance unit (metres vs raw) + fade distance
- 7 color pickers with alpha

---

## Netvar Dumper

Two dumpers for when CS:S updates and offsets shift — run either one against
the live game to get a complete offset table from your exact build. No more
forum hunting.

### `dumper.lua` (recommended — simplest)
1. CE attached to `cstrike_win64.exe`, game in an active map
2. CE → Ctrl+L (Lua Engine) → File → Open → `dumper.lua` → Execute
3. Output: `Desktop/netvar_dump.txt`

### `dumper.py` (Python via CE MCP bridge)
Needs the CE MCP bridge + TCP relay running. More debug output.
```bash
python dumper.py
```

**Known issue:** `RecvProp.m_Offset` position (`0x4C`) is estimated. If the
dump shows 0x0000 for everything, adjust that constant. The scripts print
enough debug data to calibrate.

---

## Documentation

- **`SPEC.md`** — the full verified offset spec (13 sections). Every offset
  with provenance, the correct entity stride formula, the liveness signal
  architecture, rendering patterns, and the internal reference findings.
- **`PROBE_FINDINGS.md`** — the live CE bridge probe log from the session
  where every offset was verified.

---

## Branches

- **`main`** — the known-good build. Working ESP, chams, config, presets.
  Stable, tested in-game.
- **`verified-offsets`** — probe-verified entity stride (`0x20`) +
  `m_bConnected` liveness filter + name table from player_resource. Every
  offset empirically confirmed. **Awaiting final in-game test before merge.**

---

## Credits

- **fereshetyan** — original [CS-Source-simple-esp](https://github.com/fereshetyan/CS-Source-simple-esp)
  base architecture
- **StaLLyyyy** (UC) — player resource offsets (1 Jul 2026 dump)
- **Articulador** (UC #395) — bone IDs + bone matrix offset
- **ne0h** (UC #393) — m_MoveType offset
- **arukenimon** — CS-Source-Internal reference (IsDormant pattern, head circle)
- **cssourcex64** (UC) — internal reference (netvar system, class IDs)
- **swedz** — no-flicker read/draw split pattern

---

*Disclaimer: This project is for educational purposes only. It demonstrates
external memory reading, world-to-screen projection, and overlay rendering.
Use only on local servers or environments you control.*
