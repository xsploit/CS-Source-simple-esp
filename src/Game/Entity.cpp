#include "../../include/Game/Entity.hpp"

namespace Game {
    int Entity::GetHealth() { return m_Mem.Read<int>(m_Base + Offsets::m_iHealth); }
    int Entity::GetTeam() { return m_Mem.Read<int>(m_Base + Offsets::m_iTeamNum); }
    Vector3 Entity::GetPosition() { return m_Mem.Read<Vector3>(m_Base + Offsets::m_vecOrigin); }
    Vector3 Entity::GetViewOffset() { return m_Mem.Read<Vector3>(m_Base + Offsets::m_vecViewOffset); }
    int Entity::GetFlags() { return m_Mem.Read<int>(m_Base + Offsets::m_fFlags); }
    int Entity::GetFov() { return m_Mem.Read<int>(m_Base + Offsets::m_iFov); }
    float Entity::GetFlashDuration() { return m_Mem.Read<float>(m_Base + Offsets::m_flFlashDuration); }
    float Entity::GetModelScale() { return m_Mem.Read<float>(m_Base + Offsets::m_flModelScale); }

    int Entity::GetAmmo(uintptr_t clientBase) {
        // handle & 0xFFF gives the index of the entity in the list
        uintptr_t weaponHandle = m_Mem.Read<uintptr_t>(m_Base + Offsets::m_hActiveWeapon);
        uintptr_t weaponIndex = weaponHandle & 0xFFF;
        
        // Use the same stride (0x20) as the player list
        uintptr_t weaponBase = m_Mem.Read<uintptr_t>(clientBase + Offsets::dw_BaseEntity + (weaponIndex * 0x20));
        if (!weaponBase) return -1;

        return m_Mem.Read<int>(weaponBase + Offsets::m_iClip1);
    }

    bool Entity::IsValid() {
        if (m_Base == 0) return false;
        int health = GetHealth();
        // A live player should have between 1 and 100 HP. Everything else is junk or corpses.
        return health > 0 && health <= 100;
    }
}
