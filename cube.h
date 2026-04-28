#pragma once
#include <raylib.h>
#include <array>

using std::array;

struct Cubie
{
	float size;
	Vector3 pos;
	array<Color, 6> colors;
	bool dirty;
	Cubie() = default;
	~Cubie() = default;
};

struct Cube
{
	array<array<array<Cubie, 3>, 3>, 3> cubies;
	Cube();
	~Cube() = default;
	void Draw();
};