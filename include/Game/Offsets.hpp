#pragma once
#include <windows.h>

namespace Game {
    namespace Offsets {
        // Module Bases (for reference, will be retrieved at runtime)
        // client.dll
        constexpr uintptr_t dw_BaseEntity = 0x6098C8;   // localplayer/entity list
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
        constexpr uintptr_t m_fFlags = 0x440;
        constexpr uintptr_t m_iFov = 0x1574;
        constexpr uintptr_t m_flFlashDuration = 0x1A54;
        constexpr uintptr_t m_iTeamNum = 0xD8;

        // Weapon (CBaseCombatCharacter / CBaseCombatWeapon)
        constexpr uintptr_t m_hActiveWeapon = 0x10C8;   // Handle to current weapon
        constexpr uintptr_t m_iClip1 = 0x8BC;           // Ammo in magazine
    }
}
