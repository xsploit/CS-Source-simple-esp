#pragma once
#include "../Game/Vector.hpp"

namespace Utils {
    inline bool WorldToScreen(Vector3 pos, Vector3& screen, Matrix4x4 matrix, int width, int height) {
        float w = matrix.m[3][0] * pos.x + matrix.m[3][1] * pos.y + matrix.m[3][2] * pos.z + matrix.m[3][3];
        if (w < 0.01f) return false;

        float x = matrix.m[0][0] * pos.x + matrix.m[0][1] * pos.y + matrix.m[0][2] * pos.z + matrix.m[0][3];
        float y = matrix.m[1][0] * pos.x + matrix.m[1][1] * pos.y + matrix.m[1][2] * pos.z + matrix.m[1][3];

        float nx = x / w;
        float ny = y / w;

        screen.x = (width / 2.0f) * nx + (width / 2.0f);
        screen.y = -(height / 2.0f) * ny + (height / 2.0f);
        return true;
    }
}
