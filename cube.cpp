#include "cube.h"

Cube::Cube(float size)
{
	for (int x{ -1 }; x < 2; ++x)
	{
		for (int y{ -1 }; y < 2; ++y)
		{
			for (int z{ -1 }; z < 2; ++z)
			{
				auto& cubie{ cubies[x + 1][y + 1][z + 1] };
				cubie.pos = Vector3{ 5.0f * x, 5.0f * y, 5.0f * z };
				for (int i{ 0 }; i < 6; ++i)
				{
					cubie.colors[i] =
						array{ x, y, z }[i % 3] == array{ 1, -1 }[i % 2] ?
						array{ RED, ORANGE, YELLOW, WHITE, BLUE, GREEN }[i] : BLACK;
				}
			}
		}
	}
}

void Cube::Draw()
{
	for (const auto& x : cubies)
	{
		for (const auto& y : x)
		{
			for (const auto& cubie : y)
			{
				if (cubie.dirty)
				{
					 DrawCube(cubie.pos, cubie.size, cubie.size, cubie.size, WHITE);
					 DrawCubeWires(cubie.pos, cubie.size, cubie.size, cubie.size, BLACK);
				}
			}
		}
	}
}