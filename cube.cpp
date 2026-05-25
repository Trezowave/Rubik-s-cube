#include "cube.h"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <random>
#include <fstream>
#include <algorithm>
#include <functional>
#include <optional>
#include "utility.h"
#include <map>

Cube::Cube()
{
	Reset();
	LoadState("cube_state.bin");
}

void Cube::Draw() const
{
	float stickerOffset{ (CUBIE_SIZE + FACE_THICKNESS) / 2.0f };

	for (int x{ 0 }; x < 3; ++x)
	{
		for (int y{ 0 }; y < 3; ++y)
		{
			for (int z{ 0 }; z < 3; ++z)
			{
				const auto& cubie{ cubies[x][y][z] };
				if (!cubie.dirty) continue;

				rlPushMatrix();
				if (isRotating)
				{
					if (rotAxis == 0 && x == rotLayer) rlRotatef(rotAngle, 1.0f, 0.0f, 0.0f);
					if (rotAxis == 1 && y == rotLayer) rlRotatef(rotAngle, 0.0f, 1.0f, 0.0f);
					if (rotAxis == 2 && z == rotLayer) rlRotatef(rotAngle, 0.0f, 0.0f, 1.0f);
				}

				DrawCubeV(cubie.pos, vec_all(CUBIE_SIZE), BLACK);
				for (int i{ 0 }; i < 6; ++i)
				{
					if (cubie.colors[i] == BLACK) continue;

					vec offset{};
					if		(i / 2 == 0) offset.y = SIGNS[i % 2] * stickerOffset;
					else if (i / 2 == 1) offset.x = SIGNS[i % 2] * stickerOffset;
					else if (i / 2 == 2) offset.z = SIGNS[i % 2] * stickerOffset;

					vec stickerSize{ 0.9f * vec_all(CUBIE_SIZE) };
					if		(i / 2 == 0) stickerSize.y = FACE_THICKNESS;
					else if (i / 2 == 1) stickerSize.x = FACE_THICKNESS;
					else if (i / 2 == 2) stickerSize.z = FACE_THICKNESS;

					DrawCubeV(cubie.pos + offset, stickerSize, cubie.colors[i]);
				}
				rlPopMatrix();
			}
		}
	}
}

void Cube::BakeRotation()
{
	int turns = static_cast<int>(std::round(rotAngle / 90.0f)) % 4;
	if (turns == 0) return;

	auto getPerimeterCoord
	{
		[](int side, int index)
		{
			return side % 2 != 0 ?
				(side == 1) ? index :
				(2 - index) : 2 - side;
		}
	};

	auto rotateColors 
	{
		[&](bool updateAdjacentFaces)
		{
			Color tempColors[4][3];

			auto getFaceColorIndex
			{
				[&](int side)
				{
					return updateAdjacentFaces ?
						((rotAxis + (side % 2)) * 4 + (side / 2)) % 6 :
						((rotAxis * 4) + 3) % 6 - (rotLayer / 2);
				}
			};

			auto getCubieCoords
			{
				[&](int side, int index)
				{
					int x
					{
						(rotAxis == 0) ? rotLayer :
						getPerimeterCoord((side + (rotAxis == 1 ? 3 : 0)) % 4, index)
					};
					int y
					{
						(rotAxis == 1) ? rotLayer :
						getPerimeterCoord((side + (rotAxis == 2 ? 3 : 0)) % 4, index)
					};
					int z
					{
						(rotAxis == 2) ? rotLayer :
						getPerimeterCoord((side + (rotAxis == 0 ? 3 : 0)) % 4, index)
					};
					return std::make_tuple(x, y, z);
				}
			};

			for (int side{ 0 }; side < 4; ++side)
			{
				int colorIdx{ getFaceColorIndex(side) };
				for (int i{ 0 }; i < 3; ++i)
				{
					auto [x, y, z] {getCubieCoords(side, i)};
					tempColors[side][i] = cubies[x][y][z].colors[colorIdx];
				}
			}

			for (int side{ 0 }; side < 4; ++side)
			{
				int sourceSide{ (side - turns + 4) % 4 };
				int colorIdx{ getFaceColorIndex(side) };
				for (int i{ 0 }; i < 3; ++i)
				{
					auto [x, y, z] {getCubieCoords(side, i)};
					cubies[x][y][z].colors[colorIdx] = tempColors[sourceSide][i];
					if (updateAdjacentFaces) cubies[x][y][z].dirty = true;
				}
			}
		}
	};

	rotateColors(true);
	if (rotLayer == 0 ||
		rotLayer == 2) rotateColors(false);
}

void Cube::RotateLayer(int axis, int layer, float angle)
{
	this->rotAxis	 =  axis;
	this->rotLayer	 = layer;
	this->rotAngle	 = angle;
	this->isRotating =  true;

	this->BakeRotation();

	this->rotAxis	 =    -1;
	this->rotLayer	 =	  -1;
	this->rotAngle	 =  0.0f;
	this->isRotating = false;
}

void Cube::RotateEntireCube(int axis, float angle)
{
	for (int layer{ 0 }; layer < 3; ++layer)
		RotateLayer(axis, layer, angle);
}

bool Cube::isSolved() const
{
	for (int i{ 0 }; i < 6; ++i)
	{
		Color currentFaceColor{ 0, 0, 0, 0 };
		bool colorSet{ false };
		for (const auto& layerX : cubies)
			for (const auto& layerY : layerX)
				for (const auto& cubie : layerY)
				{
					const auto& c{ cubie.colors[i] };
					if (c.a == 0 || (c.r == 0 && c.g == 0 && c.b == 0)) continue;
					if (!colorSet) { currentFaceColor = c; colorSet = true; }
					else if (c != currentFaceColor) return false;
				}
	}
	return true;
}

bool Cube::isValid() const
{
	array colorCount{ 0, 0, 0, 0, 0, 0 };
	for (const auto& layerX : cubies)
		for (const auto& layerY : layerX)
			for (const auto& cubie : layerY)
			{
				array pieceColorCount{ 0, 0, 0, 0, 0, 0 };
				for (int i{ 0 }; i < 6; ++i)
				{
					const auto& c{ cubie.colors[i] };
					if (c.a == 0 || (c.r == 0 && c.g == 0 && c.b == 0)) continue;
					int cIdx{ -1 };
					for (int j{ 0 }; j < 6; ++j)
						if (c == FACE_COLORS[j]) { cIdx = j; break; }
					if (cIdx != -1)
					{
						colorCount[cIdx]++;
						pieceColorCount[cIdx]++;
						if (pieceColorCount[cIdx] > 1) return false;
					}
				}
			}
	for (int i{ 0 }; i < 6; ++i)
		if (colorCount[i] != 9) return false;
	return true;
}

void Cube::Reset()
{
	for (int x{ -1 }; x < 2; ++x)
		for (int y{ -1 }; y < 2; ++y)
			for (int z{ -1 }; z < 2; ++z)
			{
				auto& cubie{ cubies[x + 1][y + 1][z + 1] };
				cubie.pos = vec
				{
					static_cast<float>(x),
					static_cast<float>(y),
					static_cast<float>(z)
				} * CUBIE_SIZE;

				array asis{ y, x, z };
				for (int i{ 0 }; i < 6; ++i)
					cubie.colors[i] =
					asis[i / 2] == SIGNS[i % 2] ?
					FACE_COLORS[i] : BLACK;
				cubie.dirty = true;
			}
}

void Cube::Solve(std::deque<MoveAction>& actionQueue)
{
	actionQueue.clear();

	if (!isSolved())
	{
		auto moves{ solver.solve(*this) };
		for (const auto& m : moves)
		{
			int mIdx{ static_cast<int>(m) };
			int face{ mIdx / 3 };
			int pow{ (mIdx % 3) + 1 };

			int axis{ 0 };
			int layer{ 0 };
			float targetAngle{ 0.0f };

			if		(face == 0) { axis = 1; layer = 2; targetAngle = -90.0f; } // U
			else if (face == 1) { axis = 0; layer = 2; targetAngle = -90.0f; } // R
			else if (face == 2) { axis = 2; layer = 2; targetAngle = -90.0f; } // F
			else if (face == 3) { axis = 1; layer = 0; targetAngle = 90.0f; } // D
			else if (face == 4) { axis = 0; layer = 0; targetAngle = 90.0f; } // L
			else if (face == 5) { axis = 2; layer = 0; targetAngle = 90.0f; } // B

			targetAngle *= static_cast<float>(pow);
			if (pow == 3) targetAngle = -targetAngle / 3.0f;

			actionQueue.push_back(MoveAction{ axis, layer, targetAngle });
		}
	}
}

void Cube::Shuffle(int steps)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> disAxis (0, 2);
	std::uniform_int_distribution<> disLayer(0, 1);
	std::uniform_int_distribution<> disDir  (0, 1);

	for (int i{ 0 }; i < steps; ++i)
	{
		int axis { disAxis (gen) };
		int layer{ disLayer(gen) ? 0 : 2 };
		float angle{ disDir(gen) ? 90.0f : -90.0f };
		RotateLayer(axis, layer, angle);
	}
}

void Cube::SaveState(const std::string& filename) const
{
	std::ofstream out{ filename, std::ios::binary };
	if (out) out.write(reinterpret_cast<const char*>(&cubies), sizeof(cubies));
}

bool Cube::LoadState(const std::string& filename)
{
	std::ifstream in{ filename, std::ios::binary };
	if (!in) return false;
	in.read(reinterpret_cast<char*>(&cubies), sizeof(cubies));
	return in.good();
}