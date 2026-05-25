#pragma once
#include <raylib.h>
#include <array>
#include "utility.h"

using std::array;

constexpr float sqrt3{ 1.7320508f };
constexpr float sqrt6{ 2.4494897f };

constexpr int FPS{ 240 };
constexpr int SCREEN_WIDTH{ 1000 };
constexpr int SCREEN_HEIGHT{ 1000 };
constexpr float ANIM_SPEED{ 1.0f };
constexpr float CUBIE_SIZE{ 8.0f };
constexpr float FACE_THICKNESS{ 0.2f };
constexpr float CAMERA_FOV{ 50.0f };
constexpr vec CAMERA_POS{ 50.0f, 50.0f, 50.0f };
constexpr vec CAMERA_TARGET{ 0.0f, 0.0f, 0.0f };
constexpr vec CAMERA_UP{ 0.0f, 1.0f, 0.0f };
constexpr CameraProjection CAMERA_PROJ{ CAMERA_ORTHOGRAPHIC };


constexpr float SCREEN_CENTER_X{ SCREEN_WIDTH / 2.0f };
constexpr float SCREEN_CENTER_Y{ SCREEN_HEIGHT / 2.0f };
constexpr float PX_PER_UNIT{ SCREEN_HEIGHT / CAMERA_FOV };
constexpr float CUBIE_PROJ_SIZE{ sqrt6 * CUBIE_SIZE * PX_PER_UNIT };

constexpr array FACE_COLORS
{
	YELLOW, WHITE, RED, ORANGE, BLUE, GREEN
};

constexpr array CUBE_PROJ
{
	Vector2
	{
		SCREEN_CENTER_X,
		SCREEN_CENTER_Y - CUBIE_PROJ_SIZE
	},
	Vector2
	{
		SCREEN_CENTER_X + sqrt3 / 2 * CUBIE_PROJ_SIZE,
		SCREEN_CENTER_Y - CUBIE_PROJ_SIZE / 2
	},
	Vector2
	{
		SCREEN_CENTER_X + sqrt3 / 2 * CUBIE_PROJ_SIZE,
		SCREEN_CENTER_Y + CUBIE_PROJ_SIZE / 2
	},
	Vector2
	{
		SCREEN_CENTER_X,
		SCREEN_CENTER_Y + CUBIE_PROJ_SIZE
	},
	Vector2
	{
		SCREEN_CENTER_X - sqrt3 / 2 * CUBIE_PROJ_SIZE,
		SCREEN_CENTER_Y + CUBIE_PROJ_SIZE / 2
	},
	Vector2
	{
		SCREEN_CENTER_X - sqrt3 / 2 * CUBIE_PROJ_SIZE,
		SCREEN_CENTER_Y - CUBIE_PROJ_SIZE / 2
	}
};