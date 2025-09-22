#pragma once
#ifndef S2UK_VecMath
#define S2UK_VecMath

#include <cmath>
#include <string>
#include <format>


# define M_PI           3.14159265358979323846

struct Vec2 {
    double x{};
    double y{};

    Vec2() = default;
    Vec2(double x_, double y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& other) const { return { x + other.x, y + other.y }; }
    Vec2 operator-(const Vec2& other) const { return { x - other.x, y - other.y }; }

    Vec2 operator*(double scalar) const { return { x * scalar, y * scalar }; }
    Vec2 operator/(double scalar) const { return { x / scalar, y / scalar }; }

    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    Vec2& operator*=(double scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(double scalar) { x /= scalar; y /= scalar; return *this; }

    std::string toString() {
        return std::format("Vec2({:.5f}, {:.5f})", x, y);
    }
};

struct Vec3 {
    double x{};
    double y{};
    double z{};

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& other) const { return { x + other.x, y + other.y, z + other.z }; }
    Vec3 operator-(const Vec3& other) const { return { x - other.x, y - other.y, z - other.z }; }

    Vec3 operator*(double scalar) const { return { x * scalar, y * scalar, z * scalar }; }
    Vec3 operator/(double scalar) const { return { x / scalar, y / scalar, z / scalar }; }

    Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    Vec3& operator*=(double scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    Vec3& operator/=(double scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    std::string toString() {
        return std::format("Vec3({:.5f}, {:.5f}, {:.5f})", x, y, z);
    }
};

struct Quaternion {
    double w;
    double x;
    double y;
    double z;

    Quaternion operator*(const Quaternion& rhs) const noexcept {
        return {
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w
        };
    }

    void normalize() noexcept {
        double n = std::sqrt(w * w + x * x + y * y + z * z);
        if (n > 0.0) {
            w /= n; x /= n; y /= n; z /= n;
        }
    }

    static Quaternion fromAxisAngle(double ax, double ay, double az, double angleRad) noexcept {
        double half = angleRad * 0.5;
        double s = std::sin(half);
        double c = std::cos(half);

        // normalize
        double len = std::sqrt(ax * ax + ay * ay + az * az);
        if (len == 0.0) return { 1.0, 0.0, 0.0, 0.0 };
        double nx = ax / len, ny = ay / len, nz = az / len;

        return { c, nx * s, ny * s, nz * s };
    }

    std::string toString() {
        return std::format("Quaternion({:.5f}, {:.5f}, {:.5f}, {:.5f})", w, x, y, z);
    }
};

class s2uk_vecMath {
private:
    static double degreesToRadians(double degrees) {
        return degrees * (M_PI / 180);
    }
public:
    static Quaternion eulerToQuaternion(Vec3& in) noexcept {
        const bool degrees = true;

        double yaw = degrees ? degreesToRadians(in.x) : in.x;
        double pitch = degrees ? degreesToRadians(in.y) : in.y;
        double roll = degrees ? degreesToRadians(in.z) : in.z;

        // -Y (0,1,0) = yaw, X (1,0,0) = pitch, -Z (0,0,1) = roll
        Quaternion q_yaw = Quaternion::fromAxisAngle(0.0, -1.0, 0.0, yaw);
        Quaternion q_pitch = Quaternion::fromAxisAngle(1.0, 0.0, 0.0, pitch);
        Quaternion q_roll = Quaternion::fromAxisAngle(0.0, 0.0, -1.0, roll);

        Quaternion q = q_yaw * q_pitch * q_roll;
        q.normalize();
        return q;
    }
};

class Vec3EMA {
public:
    Vec3EMA(float alpha = 0.3f)
        : alpha(alpha), initialized(false), smoothed() {
    }

    Vec3 update(const Vec3& newValue) {
        if (!initialized) {
            smoothed = newValue;
            initialized = true;
        }
        else {
            smoothed = smoothed * (1.0f - alpha) + newValue * alpha;
        }
        return smoothed;
    }

    void setAlpha(float newAlpha) { alpha = newAlpha; }
    void reset() { initialized = false; }

private:
    float alpha;
    bool initialized;
    Vec3 smoothed;
};
#endif
