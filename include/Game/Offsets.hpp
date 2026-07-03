#pragma once
#include <windows.h>

// CS:S v93 x64 offsets — dual-sourced and cross-checked:
//   older v93 dumps (alanopastel/ne0h/Articulador, Dec 2025 - Jan 2026) for the
//   fields that haven't moved, PLUS the fresh 1-Jul-2026 StaLLyyyy dump which
//   resolved several long-standing wrong values and one IMPOSSIBLE field.
//
// KEY CORRECTIONS from the StaLLyyyy dump (the things that were wrong before):
//   m_vecVelocity  0x434 → 0x148   (0x434 was a structural guess; real netvar)
//   entity stride  0x10 → 0x20     (0x10 was skipping every other entity!)
//   m_bDormant     REMOVED         (not a RecvProp in this build — externally
//                                   unreadable. every dormant offset we tried
//                                   was reading garbage. this is why the
//                                   camper/corpse filter never worked via dormant.)
// NEW ground-truth liveness signal: the player_resource m_bConnected bool table.
namespace Game {
    namespace Offsets {
        // Module Bases (for reference, will be retrieved at runtime)
        // client.dll
        constexpr uintptr_t dw_BaseEntity       = 0x6098C8; // VERIFIED by live probe — StaLyyyy's 0x6498C8 was wrong
        constexpr uintptr_t dwLocalPlayer       = 0x5F4B68; // local player
        constexpr uintptr_t dw_ABaseEntity      = 0x5AE9F8; // handle/entity entries
        constexpr uintptr_t dwPlayerResource    = 0x5FDF98; // C_PlayerResource* (NEW — liveness table)
        constexpr uintptr_t dwGlobalVars        = 0x5AE280; // CGlobalVarsBase* (NEW)

        // engine.dll
        constexpr uintptr_t dw_ViewMatrix        = 0x6A1BD0; // Projection Matrix v93 x64 (confirmed working)
        constexpr uintptr_t ClientSideViewAngles = 0x6921DC;
        constexpr uintptr_t MotionBlurrViewAngles = 0x687140;
        constexpr uintptr_t ServerSideViewAngles = 0x53E4E4;

        // Force Buttons (client.dll)
        constexpr uintptr_t ForceJump  = 0x677300;
        constexpr uintptr_t ForceDuck  = 0x677350;
        constexpr uintptr_t ForceFire  = 0x677310;
        constexpr uintptr_t ForceScope = 0x677310;

        // Netvars (CBaseEntity / CBasePlayer)
        constexpr uintptr_t m_nForceBone        = 0x7F4;
        constexpr uintptr_t m_flModelScale      = 0x8E4;
        constexpr uintptr_t m_vecViewOffset     = 0x13C;
        constexpr uintptr_t m_iHealth           = 0xD0;
        constexpr uintptr_t m_vecOrigin         = 0x428;
        constexpr uintptr_t m_vecVelocity       = 0x148;  // CORRECTED (was 0x434 — wrong)
        constexpr uintptr_t m_MoveType          = 0x1F4;  // int: 2=walk (alive player)
        constexpr uintptr_t m_lifeState         = 0xCF;   // byte: 0=alive
        // m_bDormant: NOT A NETVAR in this build. externally unreadable. removed.
        constexpr uintptr_t m_fFlags            = 0x440;
        constexpr uintptr_t m_iFov              = 0x1574;
        constexpr uintptr_t m_flFlashDuration   = 0x1A58; // StaLLyyyy (was 0x1A54 — close, corrected)
        constexpr uintptr_t m_iTeamNum          = 0xD8;
        constexpr uintptr_t m_hActiveWeapon     = 0x11A0; // StaLLyyyy (was 0x10C8 — corrected)
        constexpr uintptr_t m_iObserverMode     = 0x15DC; // NEW — for spectate handling
        constexpr uintptr_t m_hObserverTarget   = 0x15E0; // NEW — who you're spectating

        constexpr uintptr_t m_dwBoneMatrix      = 0x810;  // bone array (Articulador #395)

        // Weapon
        constexpr uintptr_t m_iClip1            = 0xC30;  // StaLLyyyy (was 0x8BC — corrected)

        // entity list layout (StaLLyyyy: stride 0x20, entity ptr at +0x8)
        constexpr uintptr_t entityStride         = 0x20;   // VERIFIED by live probe
        // entity pointer at +0x0 within each entry (NOT +0x8 — that was wrong)
        // slot formula: base + (i+1) * 0x20 for slot i (slot 0 = world)

        // player_resource tables (the liveness ground truth)
        constexpr uintptr_t pr_m_szName          = 0x790;  // const char* table, indexed by slot 1..64
        constexpr uintptr_t pr_m_bConnected      = 0x10D8; // bool table — false = cleaned-up slot
    }
}
