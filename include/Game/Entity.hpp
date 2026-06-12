#pragma once
#include <cstdint>
#include "../Core/Memory.hpp"
#include "Offsets.hpp"
#include "Vector.hpp"

namespace Game {
    class Entity {
    public:
        Entity(uintptr_t base, Core::Memory& mem) : m_Base(base), m_Mem(mem) {}

        int GetHealth();
        int GetTeam();
        Vector3 GetPosition();
        Vector3 GetViewOffset();
        int GetFlags();
        int GetFov();
        float GetFlashDuration();
        float GetModelScale();

        int GetAmmo(uintptr_t clientBase);

        uintptr_t GetBase() const { return m_Base; }

        bool IsValid();

    private:
        uintptr_t m_Base;
        Core::Memory& m_Mem;
    };
}
