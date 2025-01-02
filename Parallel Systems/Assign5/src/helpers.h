#pragma once

#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cmath>

using namespace std;

const double G = 0.0001;  // Gravitational constant
const double rlimit = 0.03; // Distance limit to avoid infinite force
const double dt = 0.005;    // Time step

const double DOMAIN_MIN_X = 0.0;
const double DOMAIN_MAX_X = 4.0;
const double DOMAIN_MIN_Y = 0.0;
const double DOMAIN_MAX_Y = 4.0;
const double OUT_OF_BOUND_MASS = -1;

struct Vec2 {
    double x, y;
    Vec2() : x(0), y(0) {}
    Vec2(double x, double y) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator*(double scalar) const { return Vec2(x * scalar, y * scalar); }
    Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    Vec2 operator/(double scalar) const { return Vec2(x / scalar, y / scalar); }

    
    double magnitude() const { return std::sqrt(x*x + y*y); }
};

