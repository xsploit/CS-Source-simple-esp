#include "../../include/Game/Entity.hpp"

namespace Game {
    int Entity::GetHealth() { return m_Mem.Read<int>(m_Base + Offsets::m_iHealth); }
    int Entity::GetTeam() { return m_Mem.Read<int>(m_Base + Offsets::m_iTeamNum); }
    uint8_t Entity::GetLifeState() { return m_Mem.Read<uint8_t>(m_Base + Offsets::m_lifeState); }
    Vector3 Entity::GetPosition() { return m_Mem.Read<Vector3>(m_Base + Offsets::m_vecOrigin); }
    Vector3 Entity::GetVelocity() { return m_Mem.Read<Vector3>(m_Base + Offsets::m_vecVelocity); }
    Vector3 Entity::GetViewOffset() { return m_Mem.Read<Vector3>(m_Base + Offsets::m_vecViewOffset); }
    int Entity::GetFlags() { return m_Mem.Read<int>(m_Base + Offsets::m_fFlags); }
    int Entity::GetFov() { return m_Mem.Read<int>(m_Base + Offsets::m_iFov); }
    float Entity::GetFlashDuration() { return m_Mem.Read<float>(m_Base + Offsets::m_flFlashDuration); }
    float Entity::GetModelScale() { return m_Mem.Read<float>(m_Base + Offsets::m_flModelScale); }

    int Entity::GetAmmo(uintptr_t clientBase) {
        // handle & 0xFFF gives the index of the entity in the list
        uintptr_t weaponHandle = m_Mem.Read<uintptr_t>(m_Base + Offsets::m_hActiveWeapon);
        uintptr_t weaponIndex = weaponHandle & 0xFFF;
        
        // entity list stride is 0x10 (v93 x64) — matches the player loop
        uintptr_t weaponBase = m_Mem.Read<uintptr_t>(clientBase + Offsets::dw_BaseEntity + (weaponIndex * 0x10));
        if (!weaponBase) return -1;

        return m_Mem.Read<int>(weaponBase + Offsets::m_iClip1);
    }

    int Entity::GetBonePositions(Vector3* out, int maxBones) {
        // Read the bone matrix pointer from the entity
        uintptr_t boneMatrixPtr = m_Mem.Read<uintptr_t>(m_Base + Offsets::m_dwBoneMatrix);
        if (!boneMatrixPtr || boneMatrixPtr < 0x10000) return 0;

        int count = maxBones < 128 ? maxBones : 128;
        for (int i = 0; i < count; i++) {
            Matrix3x4 bone = m_Mem.Read<Matrix3x4>(boneMatrixPtr + i * sizeof(Matrix3x4));
            out[i] = bone.GetOrigin();
        }
        return count;
    }

    bool Entity::IsValid() {
        if (m_Base == 0) return false;
        // m_lifeState: 0=alive, anything else is dead/dying/garbage
        if (GetLifeState() != 0) return false;
        int health = GetHealth();
        if (health <= 0 || health > 100) return false;
        int team = GetTeam();
        return team >= 2 && team <= 3;
    }
}
