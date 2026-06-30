#pragma once
#include <cmath>

struct Vector3 {
    float x, y, z;

    Vector3 operator+(const Vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vector3 operator-(const Vector3& other) const { return {x - other.x, y - other.y, z - other.z}; }
};

struct Vector2 {
    float x, y;
};

struct Matrix4x4 {
    float m[4][4];
};

// Source engine matrix3x4_t: rotation (3x3) + translation in 4th column
// Layout in memory: row0[4], row1[4], row2[4] = 12 floats = 48 bytes
struct Matrix3x4 {
    float m[3][4];

    Vector3 GetOrigin() const {
        return { m[0][3], m[1][3], m[2][3] };
    }
};
