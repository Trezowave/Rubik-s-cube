#pragma once
#include <raylib.h>
#include <raymath.h>
#include <array>
using std::array;
typedef Vector3 vec;

const std::array SIGNS{ 1, -1 };

inline vec vec_all(float val)
{
	return vec{ val, val, val };
}

inline vec operator*(const float& lhs, const vec& rhs)
{
	return rhs * lhs;
}

inline bool operator==(const Color& c1, const Color& c2)
{
	return (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a);
}

inline bool operator!=(const Color& c1, const Color& c2)
{
	return !(c1 == c2);
}

inline bool operator<(const Color& c1, const Color& c2)
{
	return std::tie(c1.r, c1.g, c1.b, c1.a) < std::tie(c2.r, c2.g, c2.b, c2.a);
}