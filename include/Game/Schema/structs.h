#pragma once
#include <cmath>

struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vector3 operator-(const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vector3 operator*(float s)          const { return { x * s, y * s, z * s }; }

    float Length()   const { return sqrtf(x * x + y * y + z * z); }
    float Length2D() const { return sqrtf(x * x + y * y); }
};

using view_matrix_t = float[4][4];

struct ScreenPos {
    float x, y;
    bool onScreen;
};

inline ScreenPos WorldToScreen(const Vector3& world, const view_matrix_t& matrix,
                                float screenW, float screenH)
{
    if (screenW <= 1.0f || screenH <= 1.0f)
        return { 0, 0, false };

    float w = matrix[3][0] * world.x + matrix[3][1] * world.y + matrix[3][2] * world.z + matrix[3][3];
    if (!std::isfinite(w) || w < 0.001f)
        return { 0, 0, false };

    float x = matrix[0][0] * world.x + matrix[0][1] * world.y + matrix[0][2] * world.z + matrix[0][3];
    float y = matrix[1][0] * world.x + matrix[1][1] * world.y + matrix[1][2] * world.z + matrix[1][3];

    float nx = x / w;
    float ny = y / w;
    if (!std::isfinite(nx) || !std::isfinite(ny) ||
        std::fabs(nx) > 8.0f || std::fabs(ny) > 8.0f)
        return { 0, 0, false };

    float sx = (screenW * 0.5f) + (nx * screenW * 0.5f);
    float sy = (screenH * 0.5f) - (ny * screenH * 0.5f);
    const bool onScreen =
        sx >= -64.0f && sx <= (screenW + 64.0f) &&
        sy >= -64.0f && sy <= (screenH + 64.0f);

    return { sx, sy, onScreen };
}
