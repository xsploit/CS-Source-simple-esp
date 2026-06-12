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
