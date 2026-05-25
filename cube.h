#pragma once
#include <raylib.h>
#include <array>
#include <deque>
#include <string>
#include "config.h"
#include "solver.h"

using std::array;
using std::deque;
using std::string;

struct MoveAction
{
	int axis{};
	int layer{};
	float angle{};
};

struct Cubie
{
	vec pos{};
	array<Color, 6> colors{};
	bool dirty{ true };
	Cubie() = default;
	~Cubie() = default;
};

struct Cube
{
	array<array<array<Cubie, 3>, 3>, 3> cubies;

	bool isRotating{ false };
	int rotAxis{ -1 };
	int rotLayer{ -1 };
	float rotAngle{ 0.0f };
	Solver solver{};

	Cube();
	~Cube() = default;
	void Draw() const;
	void BakeRotation();
	void RotateLayer(int axis, int layer, float angle);
	void RotateEntireCube(int axis, float angle);
	bool isSolved() const;
	bool isValid() const;
	void Reset();
	void Solve(deque<MoveAction>& actionQueue);
	void Shuffle(int steps = 20);

	void SaveState(const string& filename) const;
	bool LoadState(const string& filename);
};