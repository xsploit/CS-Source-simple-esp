#pragma once
#include <windows.h>

// netvar offsets below verified against UnknownCheats v93 dumps (Dec 2025 - Jan 2026):
//   m_MoveType    0x1F4   ne0h UC #393 (MOVETYPE_WALK=2)
//   dw_ViewMatrix 0x6A1BD0 alanopastel (corrected) + AADDPP #371 + Articulador #395
//   m_dwBoneMatrix 0x810  Articulador #395 (bone IDs: head=14 neck=12 pelvis=0)
//   m_vecVelocity 0x434  structural (m_vecOrigin 0x428 + 0xC; 36-byte pack to m_fFlags 0x440)
// confirmed rendering in-game: skeleton, head dot, names, fade all work.
namespace Game {
    namespace Offsets {
        // Module Bases (for reference, will be retrieved at runtime)
        // client.dll
        constexpr uintptr_t dw_BaseEntity = 0x6098C8;   // entity list
        constexpr uintptr_t dwLocalPlayer = 0x5F4B68;    // local player
        constexpr uintptr_t dw_ABaseEntity = 0x5AE9F8;  // Alternative Entity list
        
        // engine.dll
        constexpr uintptr_t dw_ViewMatrix = 0x6A1BD0;   // Projection Matrix v93 x64 (Working Offset)
        constexpr uintptr_t ClientSideViewAngles = 0x6921DC;
        constexpr uintptr_t MotionBlurrViewAngles = 0x687140;
        constexpr uintptr_t ServerSideViewAngles = 0x53E4E4;

        // Force Buttons (client.dll)
        constexpr uintptr_t ForceJump = 0x677300;
        constexpr uintptr_t ForceDuck = 0x677350;
        constexpr uintptr_t ForceFire = 0x677310;
        constexpr uintptr_t ForceScope = 0x677310;

        // Netvars (CBasePlayer)
        constexpr uintptr_t m_nForceBone = 0x7F4;
        constexpr uintptr_t m_flModelScale = 0x8E4;
        constexpr uintptr_t m_vecViewOffset = 0x13C;
        constexpr uintptr_t m_iHealth = 0xD0;           // Standard
        constexpr uintptr_t m_vecOrigin = 0x428;
        constexpr uintptr_t m_vecVelocity = 0x434;  // +0xC after origin (3 floats), before m_fFlags
        constexpr uintptr_t m_MoveType = 0x1F4;    // int: 2=walk (alive player)
        constexpr uintptr_t m_lifeState = 0xCF;   // byte: 0=alive
        constexpr uintptr_t m_bDormant = 0x214;    // byte: 0=active, 1=dormant (estimated x64)
        constexpr uintptr_t m_fFlags = 0x440;
        constexpr uintptr_t m_iFov = 0x1574;
        constexpr uintptr_t m_flFlashDuration = 0x1A54;
        constexpr uintptr_t m_iTeamNum = 0xD8;

        constexpr uintptr_t m_dwBoneMatrix = 0x810;  // pointer to matrix3x4_t[128] bone array

        // Weapon (CBaseCombatCharacter / CBaseCombatWeapon)
        constexpr uintptr_t m_hActiveWeapon = 0x10C8;   // Handle to current weapon
        constexpr uintptr_t m_iClip1 = 0x8BC;           // Ammo in magazine
    }
}
