#include "input.h"
#include <raymath.h>
#include <cmath>
#include "cube.h"
#include "config.h"

extern Cube cube;
extern bool recognizerActive;

enum class DragAxis { NONE, HORIZONTAL, VERTICAL, CUBE_LAYER };

static auto baseRot{ QuaternionIdentity() };
static float activeAngle{ 0.0f };
static float targetAngle{ 0.0f };
static vec activeAxis{ 0.0f, 1.0f, 0.0f };

void ResetCameraView(Camera& camera)
{
	baseRot = QuaternionIdentity();
	activeAngle = 0.0f;
	targetAngle = 0.0f;
	activeAxis = vec{ 0.0f, 1.0f, 0.0f };
	camera.position = CAMERA_POS;
	camera.up = CAMERA_UP;
}

bool isMouseInCube(const Vector2& mousePos)
{
	bool hasPositive{ false };
	bool hasNegative{ false };

	for (int i{ 0 }; i < 6; ++i)
	{
		double cp = Vector2CrossProduct(
			CUBE_PROJ[(i + 1) % 6] - CUBE_PROJ[i],
			mousePos - CUBE_PROJ[i]);

		if (cp > 0) hasPositive = true;
		if (cp < 0) hasNegative = true;

		if (hasPositive && hasNegative) return false;
	}

	return true;
}

void handleInput(
	Camera& camera,
	bool blockMouse,
	bool isPaintMode,
	bool isSolvingAnim,
	int paintColorIdx)
{
	if (recognizerActive || isSolvingAnim) return; 

	static int startZoneX{ 0 };
	static int startZoneY{ 0 };

	static bool dragOnCube{ false };
	static int hitLayerX{ -1 };
	static int hitLayerY{ -1 };
	static int hitLayerZ{ -1 };
	static vec hitFaceNormal{ 0.0f, 0.0f, 0.0f };

	static bool isMouseDown{ false };
	static Vector2 mouseDownPos{};

	static auto lockedAxis{ DragAxis::NONE };

	if (!isMouseDown &&
		std::abs(activeAngle) < 0.01f &&
		std::abs(targetAngle) < 0.01f)
	{
		bool isFaceF{ IsKeyDown(KEY_F) };
		bool isFaceR{ IsKeyDown(KEY_R) };
		bool isFaceT{ IsKeyDown(KEY_T) };

		if (isFaceF || isFaceR || isFaceT)
		{
			int key{ -1 };
			if (IsKeyPressed(KEY_KP_7) ||
				IsKeyPressed(KEY_SEVEN)) key = 7;
			if (IsKeyPressed(KEY_KP_8) ||
				IsKeyPressed(KEY_EIGHT)) key = 8;
			if (IsKeyPressed(KEY_KP_9) ||
				IsKeyPressed(KEY_NINE))  key = 9;
			if (IsKeyPressed(KEY_KP_4) ||
				IsKeyPressed(KEY_FOUR))  key = 4;
			if (IsKeyPressed(KEY_KP_6) ||
				IsKeyPressed(KEY_SIX))   key = 6;
			if (IsKeyPressed(KEY_KP_1) ||
				IsKeyPressed(KEY_ONE))   key = 1;
			if (IsKeyPressed(KEY_KP_2) ||
				IsKeyPressed(KEY_TWO))   key = 2;
			if (IsKeyPressed(KEY_KP_3) ||
				IsKeyPressed(KEY_THREE)) key = 3;

			if (key != -1)
			{
				dragOnCube = true;
				cube.isRotating = true;

				auto origin
				{
					GetWorldToScreen(vec{ 0.0f, 0.0f, 0.0f }, camera)
				};
				auto getProj
				{
					[&](vec v)
					{
						auto p
						{ 
							Vector2Subtract(GetWorldToScreen(v, camera), origin)
						};
						float len{ Vector2Length(p) };
						return 
							len > 0.0001f ?
							Vector2{ p.x / len, p.y / len } :
							Vector2{ 0, 0 };
					}
				};
				Vector2 proj[3]
				{
					getProj({1,0,0}),
					getProj({0,1,0}),
					getProj({0,0,1})
				};

				Vector2 refDir{ 0, 0 };
				if (isFaceF) refDir = {  0.0f, -1.0f };
				if (isFaceR) refDir = { -1.0f,  0.5f };
				if (isFaceT) refDir = {  1.0f,  0.5f };

				int bestAxis{ 0 };
				float maxDot
				{
					std::abs(Vector2DotProduct(proj[0], refDir))
				};
				float sign
				{
					Vector2DotProduct(proj[0], refDir) > 0 ?
					1.0f : -1.0f
				};

				for (int i{ 1 }; i < 3; ++i)
				{
					float d
					{
						Vector2DotProduct(proj[i], refDir)
					};
					if (std::abs(d) > maxDot)
					{
						maxDot = std::abs(d);
						bestAxis = i;
						sign = d > 0 ? 1.0f : -1.0f;
					}
				}

				cube.rotAxis = bestAxis;

				int   visLayer{ 0 };
				float visAngle{ 0.0f };

				if (isFaceF)
				{
					if (key == 1 || key == 2 || key == 3) visLayer = -1;
					if (key == 4 || key == 5 || key == 6) visLayer =  0;
					if (key == 7 || key == 8 || key == 9) visLayer =  1;

					if (key == 1 || key == 4 || key == 7) visAngle = -90.0f;
					if (key == 3 || key == 6 || key == 9) visAngle =  90.0f;
				}
				else if (isFaceR)
				{
					if (key == 1 || key == 4 || key == 7) visLayer =  1;
					if (key == 2 || key == 5 || key == 8) visLayer =  0;
					if (key == 3 || key == 6 || key == 9) visLayer = -1;

					if (key == 7 || key == 8 || key == 9) visAngle =  90.0f;
					if (key == 1 || key == 2 || key == 3) visAngle = -90.0f;
				}
				else if (isFaceT)
				{
					if (key == 1 || key == 4 || key == 7) visLayer = -1;
					if (key == 2 || key == 5 || key == 8) visLayer =  0;
					if (key == 3 || key == 6 || key == 9) visLayer =  1;

					if (key == 7 || key == 8 || key == 9) visAngle = -90.0f;
					if (key == 1 || key == 2 || key == 3) visAngle =  90.0f;
				}

				cube.rotLayer = visLayer * static_cast<int>(sign) + 1;
				targetAngle = visAngle * sign;
			}
		}
		else
		{
			int camAxis{ -1 };
			if      (IsKeyPressed(KEY_LEFT))
			{ camAxis = 1; targetAngle =  90.0f; }
			else if (IsKeyPressed(KEY_RIGHT))
			{ camAxis = 1; targetAngle = -90.0f; }
			else if (IsKeyPressed(KEY_UP))
			{
				if (IsKeyDown(KEY_LEFT_ALT) ||
					IsKeyDown(KEY_RIGHT_ALT))
				{ camAxis = 2; targetAngle = -90.0f; }
				else
				{ camAxis = 0; targetAngle =  90.0f; }
			}
			else if (IsKeyPressed(KEY_DOWN))
			{
				if (IsKeyDown(KEY_LEFT_ALT) ||
					IsKeyDown(KEY_RIGHT_ALT))
				{ camAxis = 2; targetAngle =  90.0f; }
				else
				{ camAxis = 0; targetAngle = -90.0f; }
			}

			if (camAxis != -1)
			{
				dragOnCube = false;
				if		(camAxis == 0) activeAxis = vec{ 1.0f, 0.0f, 0.0f };
				else if (camAxis == 1) activeAxis = vec{ 0.0f, 1.0f, 0.0f };
				else if (camAxis == 2) activeAxis = vec{ 0.0f, 0.0f, 1.0f };
			}
		}
	}

	if (!isMouseDown &&
		IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
		std::abs(targetAngle) < 0.01f &&
		!blockMouse)
	{
		isMouseDown = true;
		mouseDownPos = GetMousePosition();
		lockedAxis = DragAxis::NONE;
		activeAngle = 0.0f;

		Ray ray{ GetMouseRay(mouseDownPos, camera) };
		float closestDist{ 99999.0f };
		dragOnCube = false;

		for (int x{ 0 }; x < 3; ++x)
		{
			for (int y{ 0 }; y < 3; ++y)
			{
				for (int z{ 0 }; z < 3; ++z)
				{
					vec pos{ cube.cubies[x][y][z].pos };
					BoundingBox box
					{
						Vector3Subtract(pos, vec_all(CUBIE_SIZE / 2.0f)),
						Vector3Add	   (pos, vec_all(CUBIE_SIZE / 2.0f))
					};
					RayCollision coll{ GetRayCollisionBox(ray, box) };
					if (coll.hit && coll.distance < closestDist)
					{
						closestDist = coll.distance;
						hitLayerX = x;
						hitLayerY = y;
						hitLayerZ = z;
						hitFaceNormal = coll.normal;
						dragOnCube = true;
					}
				}
			}
		}

		startZoneX = mouseDownPos.x < SCREEN_CENTER_X ? -1 :  1;
		startZoneY = mouseDownPos.y < SCREEN_CENTER_Y ?  1 : -1;
	}
	else if (isMouseDown &&
		IsMouseButtonUp(MOUSE_BUTTON_LEFT))
	{
		isMouseDown = false;
		auto curMousePos{ GetMousePosition() };
		Vector2 rawDragVec
		{ 
			curMousePos.x - mouseDownPos.x,
			curMousePos.y - mouseDownPos.y
		};

		if (isPaintMode &&
			dragOnCube &&
			Vector2Length(rawDragVec) < 5.0f)
		{
			int faceIdx{ -1 };
			if		(hitFaceNormal.x >  0.5f) faceIdx = 0;
			else if (hitFaceNormal.y < -0.5f) faceIdx = 1;
			else if (hitFaceNormal.z >  0.5f) faceIdx = 2;
			else if (hitFaceNormal.x < -0.5f) faceIdx = 3;
			else if (hitFaceNormal.y >  0.5f) faceIdx = 4;
			else if (hitFaceNormal.z < -0.5f) faceIdx = 5;

			if (faceIdx != -1)
			{
				cube.cubies
					[hitLayerX]
					[hitLayerY]
					[hitLayerZ].colors[faceIdx] =
					FACE_COLORS[paintColorIdx];
			}

			activeAngle = 0.0f;
			targetAngle = 0.0f;
			cube.isRotating = false;
			lockedAxis = DragAxis::NONE;
			return;
		}

		if (dragOnCube)
		{
			targetAngle = std::round(activeAngle / 90.0f) * 90.0f;
			if (std::abs(activeAngle) > 15.0f &&
				std::abs(targetAngle) < 1.0f) targetAngle =
				activeAngle > 0.0f ? 90.0f : -90.0f;
		}
		else
		{
			Vector2 mouseUpPos{ GetMousePosition() };
			int endZoneX{ mouseUpPos.x < SCREEN_CENTER_X ? -1 :  1 };
			int endZoneY{ mouseUpPos.y < SCREEN_CENTER_Y ?  1 : -1 };

			bool isCrossed{ false };
			if (lockedAxis == DragAxis::HORIZONTAL &&
				startZoneX != endZoneX) isCrossed = true;
			else if (lockedAxis == DragAxis::VERTICAL &&
				startZoneY != endZoneY) isCrossed = true;

			if (isCrossed && std::abs(activeAngle) < 45.0f &&
				std::abs(activeAngle) > 0.1f) targetAngle =
				activeAngle > 0.0f ? 90.0f : -90.0f;
			else targetAngle = std::round(activeAngle / 90.0f) * 90.0f;
		}
	}

	auto updateCamera = [&]()
		{
			auto deltaRot
			{
				QuaternionFromAxisAngle(activeAxis, activeAngle * DEG2RAD)
			};
			auto finalRot{ QuaternionMultiply(baseRot, deltaRot) };
			camera.position = Vector3RotateByQuaternion(CAMERA_POS, finalRot);
			camera.up = Vector3RotateByQuaternion(CAMERA_UP, finalRot);
		};

	if (isMouseDown)
	{
		auto curMousePos{ GetMousePosition() };
		Vector2 rawDragVec
		{
			curMousePos.x - mouseDownPos.x,
			curMousePos.y - mouseDownPos.y
		};
		Vector2 mathDragVec{ rawDragVec.x, -rawDragVec.y };

		if (lockedAxis == DragAxis::NONE &&
			Vector2Length(rawDragVec) > 5.0f)
		{
			if (dragOnCube)
			{
				int axisA{ -1 };
				int axisB{ -1 };
				if		(std::abs(hitFaceNormal.x) > 0.5f)
				{ axisA = 1; axisB = 2; }
				else if (std::abs(hitFaceNormal.y) > 0.5f)
				{ axisA = 0; axisB = 2; }
				else
				{ axisA = 0; axisB = 1; }

				auto getScreenDir
				{
					[&](int rotAxis) -> Vector2
					{
						vec rotAxis3D{ 0.0f, 0.0f, 0.0f };
						if		(rotAxis == 0) rotAxis3D.x = 1.0f;
						else if (rotAxis == 1) rotAxis3D.y = 1.0f;
						else if (rotAxis == 2) rotAxis3D.z = 1.0f;

						vec move3D
						{
							Vector3CrossProduct(rotAxis3D, hitFaceNormal)
						};

						auto origin
						{
							GetWorldToScreen(vec{0.0f, 0.0f, 0.0f}, camera)
						};
						auto tip{ GetWorldToScreen(move3D, camera) };
						return Vector2Subtract(tip, origin);
					}
				};

				auto dirA{ getScreenDir(axisA) };
				auto dirB{ getScreenDir(axisB) };

				float weightA
				{
					std::abs(
						rawDragVec.x * dirB.y -
						rawDragVec.y * dirB.x)
				};
				float weightB
				{
					std::abs(
						dirA.x * rawDragVec.y -
						dirA.y * rawDragVec.x)
				};

				cube.rotAxis = weightA > weightB ? axisA : axisB;
				cube.rotLayer = 
					cube.rotAxis == 0 ?
					hitLayerX : (
						cube.rotAxis == 1 ?
						hitLayerY : hitLayerZ);

				lockedAxis = DragAxis::CUBE_LAYER;
				cube.isRotating = true;
			}
			else
			{
				if (std::abs(mathDragVec.x) >
					std::abs(mathDragVec.y))
				{
					lockedAxis = DragAxis::HORIZONTAL;
					activeAxis = vec{ 0.0f, 1.0f, 0.0f };
				}
				else {
					lockedAxis = DragAxis::VERTICAL;
					activeAxis = (startZoneX == -1) ?
						vec{ 1.0f, 0.0f, 0.0f } :
						vec{ 0.0f, 0.0f, 1.0f };
				}
			}
		}

		if (lockedAxis != DragAxis::NONE)
		{
			if (lockedAxis == DragAxis::CUBE_LAYER)
			{
				vec rotAxis3D{ 0.0f, 0.0f, 0.0f };
				if		(cube.rotAxis == 0) rotAxis3D.x = 1.0f;
				else if (cube.rotAxis == 1) rotAxis3D.y = 1.0f;
				else if (cube.rotAxis == 2) rotAxis3D.z = 1.0f;

				vec move3D
				{
					Vector3CrossProduct(rotAxis3D, hitFaceNormal)
				};
				auto origin
				{
					GetWorldToScreen(vec{0.0f, 0.0f, 0.0f}, camera)
				};
				auto tip{ GetWorldToScreen(move3D, camera) };
				auto dir2D{ Vector2Subtract(tip, origin) };

				float len{ Vector2Length(dir2D) };
				if (len > 0.0001f)
				{
					float projectedDrag
					{ 
						Vector2DotProduct(rawDragVec, dir2D) / len
					};
					activeAngle = projectedDrag * 0.18f;
				}
				cube.rotAngle = activeAngle;
			}
			else
			{
				if (lockedAxis == DragAxis::HORIZONTAL)
				{
					activeAngle = -mathDragVec.x * 0.18f;
				}
				else if (lockedAxis == DragAxis::VERTICAL)
				{
					activeAngle =
						startZoneX == -1 ?
						mathDragVec.y *  0.18f :
					    mathDragVec.y * -0.18f;
				}
				updateCamera();
			}
		}
	}
	else
	{
		const float ANIM_SPEED{ 0.15f };

		if (std::abs(targetAngle - activeAngle) > 0.05f)
		{
			activeAngle +=
				(targetAngle - activeAngle) * ANIM_SPEED;
			if (dragOnCube) cube.rotAngle = activeAngle;
			else updateCamera();
		}
		else if (targetAngle != 0.0f ||
			activeAngle != 0.0f)
		{
			activeAngle = targetAngle;

			if (dragOnCube)
			{
				cube.rotAngle = activeAngle;
				cube.BakeRotation();
				cube.isRotating = false;
			}
			else
			{
				updateCamera();
				auto deltaRot
				{
					QuaternionFromAxisAngle(activeAxis, activeAngle * DEG2RAD)
				};
				baseRot = QuaternionMultiply(baseRot, deltaRot);
			}

			activeAngle = 0.0f;
			targetAngle = 0.0f;
		}
	}
}