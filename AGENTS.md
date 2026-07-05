# CS:Source ESP — Agent Rules

Lessons learned the hard way. Read before touching offsets or the entity filter.

## The Three Mistakes I Keep Making

### 1. NEVER remove the stale filter without confirming the replacement works for DEATH
- `m_bConnected` catches DISCONNECTION, not death. A dead player is still
  connected to the server. It stays 1 until they fully leave.
- `m_bDormant` is NOT a netvar in CS:S v93 x64 — externally unreadable.
  Do not try dormant offsets. They are all garbage. This has been proven
  three times across multiple commits.
- The stale-position+HP filter is the ONLY working death detection we have.
  It has the camper ambiguity (a camper at full HP looks like a corpse at
  full HP) but it's better than nothing.
- Before removing it: confirm the replacement catches BOTH "round-end corpse"
  AND "disconnected player." If you haven't seen the death case work, do not
  remove the stale filter.

### 2. NEVER trust an offset you haven't read from live memory
- Forum dumps disagree with each other. StaLyyyy's entity list base (0x6498C8)
  was wrong for our build. Our live probe confirmed 0x6098C8.
- If you're about to use an offset, it must either:
  (a) have a probe entry in PROBE_FINDINGS.md showing it was read from the
      live game via CE bridge, OR
  (b) be confirmed by successful in-game rendering
- Unverified offsets go in the "❌ UNVERIFIED" section of SPEC.md until probed.

### 3. NEVER change the entity stride formula without re-reading the probe data
- The correct formula is: `base + i * 0x20` (i starts at 1)
- slot 0 (world) at base+0x00, slot 1 at base+0x20, slot 2 at base+0x40
- entity pointer is the qword at the start of each 0x20-byte entry (+0x0)
- I broke this TWICE with (i+1)*0x20 (skipped slot 1) and +0x8 (read past
  the entity pointer into metadata). Don't change it again.

## Verified Offsets (do not change without re-probing)

These have been read from live memory via CE bridge AND/OR confirmed by
successful in-game rendering. They are correct for CS:S v93 x64.

| field | offset | source |
|-------|--------|--------|
| entity list base | client.dll + 0x6098C8 | live probe |
| entity stride | 0x20, ptr at +0x0 | live probe |
| entity slot formula | base + i*0x20 (i=1..127) | live probe |
| m_iHealth | +0xD0 | live probe |
| m_iTeamNum | +0xD8 | live probe |
| m_vecOrigin | +0x428 | live probe |
| m_vecVelocity | +0x148 | live probe (moving bots) |
| m_lifeState | +0xCF | in-game render |
| m_fFlags | +0x440 | in-game render |
| m_vecViewOffset | +0x13C | in-game render |
| m_nModelIndex | +0xCC | in-game render |
| m_MoveType | +0x1F4 | in-game render |
| m_dwBoneMatrix | +0x810 | in-game render |
| dwViewMatrix | engine.dll + 0x6A1BD0 | in-game render |
| dwPlayerResource | client.dll + 0x5FDF98 | live probe |
| m_bConnected | player_resource + 0x10D8 | live probe (catches disconnect, NOT death) |
| m_szName | player_resource + 0x790 | live probe |

## What Does NOT Work

- `m_bDormant` at ANY offset — not a netvar, externally unreadable, abandoned
- `m_vecVelocity` at 0x434 — wrong, reads [0,0,0] always
- entity stride 0x10 — reads alternating entity/metadata garbage
- entity list base 0x6498C8 — wrong table
- name list at 0x609D68 + 0x798 — old hardcoded hack, use player_resource

## Architecture Rules

- Read/draw split: all RPM in pass 1 (vector<RenderRecord>), all ImGui in
  pass 2. Never interleave. Prevents half-drawn frames.
- Batch RPM: one bulk read per entity (0x900 bytes), one bulk read per bone
  array (1536 bytes). Do NOT revert to per-field reads.
- m_bConnected is for DISCONNECT filtering, the stale filter is for DEATH
  filtering. They work together, not as replacements for each other.

## Still Unverified (TODO)

- `CCSPlayer::deadflag` — netvar from cssourcex64 source, could be the
  real death signal. Needs a live probe while a bot is dead.
- `m_flSimulationTime` — freezes on death, advances for live players.
  Needs a live probe.
- RecvProp tree-walking dumper — offsets are hierarchical (relative to
  parent sub-table), need recursive summing, not flat reads.
