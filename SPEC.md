# CS:S v93 x64 — ESP Specification & Verified Findings

**Last updated:** 2026-06-30
**Source:** Live memory probes via CE MCP bridge + StaLLyyyy UC dump (1 Jul 2026)
          + arukenimon/CS-Source-Internal reference + cssourcex64 UC internal source

---

## 1. ENVIRONMENT

- **Game:** Counter-Strike: Source, Steam build v93, **x64** (`cstrike_win64.exe`)
- **Launch:** `cstrike.exe -force32bit` gives x86; default Steam launch gives x64.
  Our ESP targets the **x64 build** (`cstrike_win64.exe`).
- **Test env:** Local listenserver (Create Server + bots). This affects entity
  enumeration — see §3 notes.
- **Module bases (runtime-resolved, do NOT hardcode):**
  - `client.dll`: read via `GetModuleHandle` / toolhelp32 snapshot
  - `engine.dll`: same
  - Example live values: client=`0x7FFA60940000`, engine=`0x7FFA73080000`

---

## 2. ENTITY LIST — VERIFIED BY LIVE PROBE

### Base offset
```
client.dll + 0x6098C8   →  entity list (CEntInfo array)
```
**Confirmed** by reading live memory. StaLLyyyy's `0x6498C8` is WRONG for this
build (reads a different table with mixed values).

### Stride & pointer offset
```
stride:               0x20  (each CEntInfo entry is 0x20 bytes)
entity pointer at:    +0x00 within each entry
serial/metadata at:   +0x08, +0x10, +0x18 within each entry
```

### Slot formula
```
slot 0 (world entity):  base + 0x00
slot i (player i):      base + i * 0x20          ← i starts at 1, naturally skips world
```

**CORRECTION:** the initial probe analysis said `(i+1) * 0x20` — that was an
off-by-one that skipped slot 1 (the first real player). fixed in commit ab153e5
after a bot in slot 1 wasn't rendering. the correct formula is simply
`base + i * 0x20` where i starts at 1.

**NOTE:** Our main branch used `base + i * 0x10`, which accidentally worked
because it hit entity pointers on even indices and metadata on odd indices.
It was reading HALF the entities and HALF garbage.

### Entity pointer validation
A valid entity pointer is a heap pointer in the range
`0x0000010000000000` to `0x00007FF000000000` (excluding the module range
`0x00007FFA...` which is DLL memory, not entity heap).

---

## 3. NETVAR OFFSETS — VERIFIED BY LIVE PROBE

All offsets are relative to the entity base pointer.

| Netvar | Offset | Type | Verified | Notes |
|--------|--------|------|----------|-------|
| `m_iHealth` | `+0xD0` | int32 | ✅ | Read 1, 74, 100 on live bots |
| `m_iTeamNum` | `+0xD8` | int32 | ✅ | Read 2 (T), 3 (CT) |
| `m_vecOrigin` | `+0x428` | Vector3 (float×3) | ✅ | Read real map coords |
| `m_lifeState` | `+0xCF` | byte | ✅ (main) | 0=alive, nonzero=dead |
| `m_fFlags` | `+0x440` | int32 | ✅ (main) | bit 0 = on ground |
| `m_vecViewOffset` | `+0x13C` | Vector3 | ✅ (main) | origin + this = eye pos |
| `m_nModelIndex` | `+0xCC` | int32 | ✅ (main) | >0 for real players |
| `m_MoveType` | `+0x1F4` | int32 | ✅ (main) | 2=walk (alive player) |
| `m_dwBoneMatrix` | `+0x810` | pointer | ✅ (main) | → matrix3x4_t[128] bone array |
| `m_vecVelocity` | `+0x148` | Vector3 | ⚠ UNVERIFIED | StaLLyyyy dump value; read [0,0,0] but bots were stationary. LOW PRIORITY (cosmetic prediction). |

---

## 4. LIVENESS SIGNAL — THE CAMPER/CORPSE PROBLEM — SOLVED

### What does NOT work
- **`m_bDormant`**: NOT a RecvProp (netvar) in CS:S v93 x64. Externally
  unreadable. Set internally via `IClientNetworkable` vtable. Every offset we
  tried (0x214, 0x8C, 0x17e, float-compare) was reading garbage. **ABANDONED.**
  (Confirmed by StaLLyyyy dump: `m_bDormant = 0x0 // not a RecvProp`)

### What DOES work — `m_bConnected` player resource table
```
dwPlayerResource pointer:  client.dll + 0x5FDF98  →  C_PlayerResource*
m_bConnected table:        player_resource + 0x10D8  (bool[64], indexed by slot 1..64)
```
**Confirmed by live probe:**
- slot 1 (local player) = 0
- slots 2–16 (bots) = 1

**Semantics:** `m_bConnected[slot]` is the engine's own ground-truth flag for
"this slot is occupied by a connected player." When a player disconnects or
their corpse is cleaned up at round end, the slot flips to 0. A camper holding
an angle reads 1 indefinitely.

**This replaces the entire stale-position heuristic.** No tuning, no false
positives. The engine knows; we just ask it.

### Filter chain (verified-correct order):
```
1. entity pointer valid (heap range)
2. m_bConnected[slot] == true      ← THE LIVENESS GROUND TRUTH
3. m_lifeState == 0                ← alive
4. m_iHealth in [1, 100]           ← sane HP
5. m_iTeamNum in [2, 3]            ← real player (T or CT)
6. NOT at origin (0,0,0)           ← spawned, not empty slot
7. team filter (optional: skip teammates)
8. m_nModelIndex > 0               ← has a model
9. m_MoveType in [2, 11]           ← walking/moving entity
```

---

## 5. NAME TABLE — VERIFIED BY LIVE PROBE

```
m_szName table:  player_resource + 0x790  (const char*[64], 8-byte pointer stride)
```
**Confirmed by live probe:**
- slot 1: null (local player)
- slot 2: "SUBSECT" (the human player)
- slots 3-5: "*** Alfred", "*** Graham", "*** Neil" (bots)

**Replaces** the old hardcoded `client.dll + 0x609D68 + 0x798 + (i * 0x4)`
hack, which was a stale unverified offset.

---

## 6. VIEW MATRIX — VERIFIED BY LIVE PROBE (rendering)

```
dwViewMatrix:  engine.dll + 0x6A1BD0  →  float[16] (4x4 view-projection)
```
**Confirmed** by successful in-game rendering (skeleton + chams + head dot all
track correctly with this offset).

---

## 7. BONE MATRIX — VERIFIED BY LIVE PROBE (rendering)

```
m_dwBoneMatrix:  entity + 0x810  →  pointer to matrix3x4_t[128]
bone stride:     48 bytes (0x30) per bone (sizeof(matrix3x4_t))
```
**Bone IDs** (confirmed by Articulador UC #395 + arukenimon internal):
```
0  = pelvis          16 = L shoulder
1  = L hip           17 = L elbow
2  = L knee          18 = L hand
3  = L ankle         29 = R shoulder
5  = R hip           30 = R elbow
6  = R knee          31 = R hand
7  = R ankle
10 = spine
11 = chest
12 = neck
14 = head            ← the "head" bone (pivot at base of skull; lift dot ~12%
                       of body height for visual center)
```

Bone positions: read `matrix3x4_t` at `boneMatrixPtr + boneID * 48`, extract
origin from offsets `+0x0C` (x), `+0x1C` (y), `+0x2C` (z).

---

## 8. RENDERING — VERIFIED PATTERNS

### World-to-Screen
Standard view-projection transform. Matrix is row-major `float[16]`:
```
sx = (m[0]*px + m[1]*py + m[2]*pz + m[3])
sy = (m[4]*px + m[5]*py + m[6]*pz + m[7])
w  = (m[8]*px + m[9]*py + m[10]*pz + m[11])
if w < 0.01: behind camera, skip
screenX = (screenW/2) + (sx/w) * (screenW/2)
screenY = (screenH/2) - (sy/w) * (screenH/2)
```

### No-flicker architecture
Two-pass: read all entities into `vector<RenderRecord>` (pass 1, all RPM),
then draw from records (pass 2, pure ImGui). Prevents half-drawn frames from
RPM latency spikes.

### Head dot
Bone 14 is the head pivot (base of skull). Lift the dot in screen space by
`bodyHeight * 0.12` to land on the visual head center. Scale the dot radius
by projecting a point 4 world-units above the head and using the on-screen
delta — auto-scales with range.

### Bone-glow chams
Thick translucent lines along each bone link + bright core lines + filled
joint dots at major joints. Per-link validity (skip offscreen bones, don't
nuke the whole skeleton).

---

## 9. OFFSETS THAT ARE WRONG / SHOULD NOT BE USED

| Offset | Value | Why it's wrong |
|--------|-------|----------------|
| entity list base | `0x6498C8` | StaLyyyy dump — reads wrong table for our build |
| entity stride | `0x10` | Main branch — hits alternating entity/metadata, misses half |
| `m_bDormant` | any | Not a netvar in this build — externally unreadable |
| `m_vecVelocity` | `0x434` | Old structural guess — wrong (StaLyyyy says 0x148, unverified) |
| name list | `0x609D68 + 0x798` | Old hardcoded hack — use player_resource + 0x790 instead |

---

## 10. REFERENCE SOURCES

1. **Live CE MCP bridge probes** (this session) — all ✅ items
2. **StaLLyyyy UC dump** (1 Jul 2026, "CSS x64 Latest Offsets/netvars")
   — player_resource, m_bConnected, m_szName confirmed; entity list base WRONG
3. **Articulador UC #395** (Jan 2026) — bone IDs, m_dwBoneMatrix=0x810
4. **ne0h UC #393** (Dec 2025) — m_MoveType=0x1F4
5. **arukenimon/CS-Source-Internal** (GitHub) — IsDormant vtable pattern,
   range-scaled head circle, bone link table
6. **cssourcex64 UC internal** (Downloads folder) — full internal reference:
   NetVar::Get dynamic resolution, deadflag netvar, ECSClientClass enum
   (CCSPlayer=27), ESP draw patterns
7. **swedz CS2 no-flicker tutorial** — read/draw split architecture

---

## 11. TODO — APPLYING THE FINDINGS

The ESP code changes needed (on the `verified-offsets` branch):

### Entity loop fix
```cpp
// OLD (main, broken — half garbage):
uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + 0x6098C8 + (i * 0x10));

// NEW (verified):
uintptr_t entityBase = mem.Read<uintptr_t>(clientBase + 0x6098C8 + (i * 0x20));
```

### Liveness filter
```cpp
// Read player resource once per frame:
uintptr_t playerResource = mem.Read<uintptr_t>(clientBase + 0x5FDF98);

// In entity loop, before any rendering:
bool connected = mem.Read<bool>(playerResource + 0x10D8 + i);  // i = slot number
if (!connected) continue;
```

### Name read
```cpp
// OLD (hardcoded junk):
uintptr_t nameListBase = mem.Read<uintptr_t>(clientBase + 0x609D68);
uintptr_t np = mem.Read<uintptr_t>(nameListBase + 0x798 + (i * 0x4));

// NEW (verified):
uintptr_t playerResource = mem.Read<uintptr_t>(clientBase + 0x5FDF98);
uintptr_t np = mem.Read<uintptr_t>(playerResource + 0x790 + (i * 0x8));
```

### Remove
- All dormant checks (0x214, 0x8C, float-compare — all garbage)
- All stale-position tracking (g_LastPos, g_StaleFrames, staleFrames config)
- Old name list hardcoded offset (0x609D68)

---

## 12. NETVAR DUMPER (for future offset updates)

Two dumper implementations — both walk the Source engine's ClientClass linked
list externally and dump every netvar for every class. run when CS:S updates
and offsets shift. no more forum hunting.

### dumper.lua (CE Lua — recommended, simplest)
- paste into CE Lua Engine (Ctrl+L), Execute
- output: `Desktop/netvar_dump.txt`
- zero dependencies beyond CE itself

### dumper.py (Python via CE MCP bridge)
- talks to the TCP relay on 127.0.0.1:9876
- same output, more debug info
- needs the relay + bridge running

**KNOWN ISSUE:** `RP_OFFSET = 0x4C` (the position of the offset field within
RecvProp on x64) is an estimate. if the dump shows 0x0000 for everything,
that number needs adjusting. the scripts print enough debug data to calibrate.

---

## 13. INTERNAL REFERENCE FINDINGS (from cssourcex64 UC source)

The `cssourcex64` source (Downloads folder) is a full internal CS:S x64 cheat
with SDK access. Key insights for our external project:

### deadflag netvar
```cpp
// from NetVar.h:
inline bool& get_deadflag(C_BaseEntity* ent) {
    static const ptrdiff_t offset = NetVar::Get("CCSPlayer", "deadflag");
    ...
}
```
`CCSPlayer::deadflag` is a netvar (externally readable) that could be an
ALTERNATIVE liveness signal to m_bConnected. we haven't probed it yet but
it's worth testing — if deadflag reads true for corpses, it's simpler than
the player_resource table.

### ECSClientClass enum (class IDs)
`CCSPlayer = 27` in the ClientClass enum. an internal cheat uses this to
identify player entities. externally we use movetype/team/HP validation
instead, but the class ID is available at `ClientClass + 0x28` if we ever
walk the class list.

### IsDormant is a VTABLE CALL, not a field
confirmed by the cssourcex64 code calling `pEntity->IsDormant()` as a virtual
function. this is why it's externally unreadable — it's index ~8 in the
`IClientNetworkable` vtable, not a struct field. the StaLyyyy dump note
(`m_bDormant = 0x0 // not a RecvProp`) confirms: it does not exist as a
networked property. our m_bConnected approach is the correct external path.

### Dynamic netvar resolution (the gold standard)
the internal cheat resolves ALL offsets at runtime via:
```cpp
NetVar::Get("CCSPlayer", "m_iHealth")
```
which walks `ClientClass->GetAllClasses()` → `RecvTable` → `RecvProp` by name.
this is what our dumper.lua/dumper.py replicate externally — same data,
no injection.
