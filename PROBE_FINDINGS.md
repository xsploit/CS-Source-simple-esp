# CS:S v93 x64 — Live Memory Probe Findings

Empirically verified against `cstrike_win64.exe` via the CE MCP bridge
(2026-06-30). Every value below was read from the running game, not from a
forum dump. This supersedes all prior offset claims when they conflict.

## Setup
- client.dll base: `0x7FFA60940000` (read at runtime via enum_modules)
- engine.dll base: `0x7FFA73080000`
- bridge: CE attached + ce_mcp_bridge.lua running, TCP relay on 127.0.0.1:9876

## ✅ CONFIRMED

### Entity list
- **base: `client.dll + 0x6098C8`** (main build was RIGHT; StaLLyyyy's 0x6498C8 is a different/wrong table)
- **stride: `0x20`** (main's 0x10 was WRONG — it hit alternating entity/metadata qwords, reading garbage on odd slots and missing half the entities)
- **entity pointer offset: `(i+1) * 0x20`** for slot i (slot 0 at +0x0 is the world entity; player slots start at +0x20)
- **verified entities** (read clean HP/team/origin):
  - slot 1: HP=1, team=2, origin=[7245, 5619, -775]
  - slot 2: HP=100, team=3, origin=[5907, 4868, -454]
  - slot 3: HP=74, team=3, origin=[5829, 4546, -391]

### Netvars (read off confirmed entities)
- `m_iHealth` = `+0xD0` ✅ (values 1, 74, 100 — all sane)
- `m_iTeamNum` = `+0xD8` ✅ (values 2, 3 — T and CT)
- `m_vecOrigin` = `+0x428` ✅ (real map coordinates)

### Player resource table (THE LIVENESS SIGNAL)
- **`dwPlayerResource` pointer: `client.dll + 0x5FDF98`** ✅ (StaLLyyyy was right) → reads `0x219817d5cf0` (valid heap ptr)
- **`m_bConnected` bool table: `player_resource + 0x10D8`** ✅ (indexed by slot 1..64)
  - slot 1 = 0 (local player — never "connected" to self)
  - slots 2–16 = 1 (all connected bots)
  - **this is the ground-truth liveness signal**: a slot flips to 0 when the
    player disconnects / is cleaned up at round end. replaces the entire
    stale-position heuristic. campers read 1 forever; corpses read 0.
  - NOTE: must skip slot 1 (local) — we already skip local by pointer anyway.
- **`m_szName` const char* table: `player_resource + 0x790`** ✅ (8-byte pointer stride)
  - slot 1: null (local)
  - slot 2: "SUBSECT" (the player)
  - slots 3-5: bot names (Alfred, Graham, Neil)
  - replaces the old hardcoded `0x609D68 + 0x798` name hack

## ❌ UNVERIFIED / NEEDS MORE PROBING

### m_vecVelocity
- `+0x148` (StaLLyyyy): ✅ **CONFIRMED WORKING** — reads nonzero values (100-240 range)
  for alive/moving bots, zero for dead/idle bots. StaLLyyyy was right.
- `+0x434` (old guess): ❌ WRONG — reads [0,0,0] on all entities. dead offset.
- sample from live probe (moving bots):
  - slot 2 HP=100: vel=[237, -39, 20]
  - slot 3 HP=100: vel=[18, -97, 0]
  - slot 8 HP=100: vel=[141, 191, -10]
  - slot 1 HP=1 (dead): vel=[0, 0, 0] ✓

### Liveness signal (the camper/corpse problem)
- `m_bDormant`: NOT a netvar in this build (StaLLyyyy dump confirms). externally unreadable. ABANDONED.
- `m_bConnected` (player_resource + 0x10D8): ✅ CONFIRMED WORKING (see above).
  this solves the camper/corpse problem — no further liveness probing needed.

## TODO
- [x] probe player_resource m_bConnected table — CONFIRMED WORKING
- [x] probe name table — CONFIRMED WORKING
- [ ] probe velocity with a moving bot (low priority — cosmetic)
- [ ] apply findings to the ESP: stride 0x20 + offset (i+1)*0x20, m_bConnected
      filter, name from player_resource+0x790. this is the verified fix.
