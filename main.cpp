#include <raylib.h>
#include <raymath.h>
#include "cube.h"

const int screenWidth{ 640 };
const int screenHeight{ 480 };
Camera3D camera{};
Cube cube{};

void init()
{
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	InitWindow(screenWidth, screenHeight, "Rubik's Cube");
	camera.position = Vector3{ 20.0f, 20.0f, 20.0f };
	camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
	camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;
	SetTargetFPS(240);
}

int WinMain(void)
{
	init();

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(WHITE);
		BeginMode3D(camera);

		// mesh
		DrawGrid(100, 5);
		// z-axis
		DrawLine3D(Vector3{ 0, 100, 0 }, Vector3 { 0, -100, 0 }, BLACK);

		cube.Draw();

		EndMode3D();
		EndDrawing();
	}

	CloseWindow();

	return 0;
}