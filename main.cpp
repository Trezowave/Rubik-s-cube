#include <csignal>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI              
    #define NOUSER             
    #include <windows.h>
#else
    #include <unistd.h>
#endif
#include <raylib.h>
#include <raymath.h>
#include <random>
#include <string>
#include <deque>
#include "config.h"
#include "cube.h"
#include "input.h"
#include "vision.h"
#ifdef _WIN32
	#include <Debug.h>
#else
	#include "/run/media/trezowave/Data/Workspace/CPP_Projects/Debug.h"
#endif

volatile sig_atomic_t exitSignal{ 0 };

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_CLOSE_EVENT ||
        ctrlType == CTRL_LOGOFF_EVENT ||
        ctrlType == CTRL_SHUTDOWN_EVENT)
    {
        exitSignal = 1;
        return TRUE;
    }
    return FALSE;
}
#endif

void SignalHandler(int signum)
{
	exitSignal = 1;
}

Camera camera
{
	CAMERA_POS,
	CAMERA_TARGET,
	CAMERA_UP,
	CAMERA_FOV,
	CAMERA_PROJ
};
Cube cube{};
bool isPaintMode{ false };
bool showInvalidStateError{ false };
array<array<array<Cubie, 3>, 3>, 3> backupCubies{};

Recognizer recognizer;
bool recognizerActive{ false };

std::deque<MoveAction> actionQueue{};
std::vector<MoveAction> historyStack{}; 
MoveAction lastProcessedMove{};        
bool isPaused{ false };
bool stepForward{ false };
bool stepBackward{ false };
bool isMovingBack{ false };

bool DrawCyberpunkButton(
	Rectangle bounds,
	const char* text,
	Color neonColor,
	bool isActive = false)
{
	bool isHover
	{
		CheckCollisionPointRec(GetMousePosition(), bounds)
	};
	bool isClicked
	{
		isHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
	};

	Color bgColor
	{
		isActive ?
		Fade(neonColor, 0.4f) :
		isHover ?
		Fade(neonColor, 0.2f) :
		Color{ 20, 20, 25, 200 }
	};
	Color lineColor
	{
		isHover || isActive ?
		WHITE : neonColor
	};

	DrawRectangleRec(bounds, bgColor);
	DrawRectangleLinesEx(bounds, 2.0f, lineColor);

	if (isActive) DrawRectangle(
		static_cast<int>(bounds.x) - 5,
		static_cast<int>(bounds.y),
		3,
		static_cast<int>(bounds.height),
		neonColor);

	int textW{ MeasureText(text, 20) };
	DrawText(
		text,
		static_cast<int>(bounds.x + bounds.width / 2 - textW / 2),
		static_cast<int>(bounds.y + bounds.height / 2 - 10),
		20,
		lineColor);

	return isClicked;
}

void init()
{
	SetTraceLogLevel(LOG_NONE);
	SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rubik's Cube");
	SetExitKey(0);
	SetTargetFPS(FPS);
}

int main(void)
{
	init();

#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif

	bool isInputtingSteps{ false };
	std::string stepInput{ "20" };
	std::deque<MoveAction> actionQueue{};
	int currentPaintColorIdx{ 0 };
	bool isSolvingAnim{ false };
	float currentSolveAngle{ 0.0f };
	float solveTargetAngle{ 0.0f };

	while (!WindowShouldClose() && exitSignal == 0)
	{
		Rectangle uiPanel
		{
			SCREEN_WIDTH - 200.0f,
			30.0f,
			180.0f,
			320.0f
		};
		Rectangle palettePanel
		{
			SCREEN_CENTER_X - 160.0f,
			SCREEN_HEIGHT - 100.0f,
			320.0f,
			80.0f
		};

		bool isQueueActive
		{
			!actionQueue.empty() ||
			isSolvingAnim
		};
		bool blockMouse
		{
			isInputtingSteps ||
			CheckCollisionPointRec(GetMousePosition(), uiPanel) ||
			isQueueActive
		};
		if (isPaintMode &&
			CheckCollisionPointRec(GetMousePosition(), palettePanel))
			blockMouse = true;
		if (recognizerActive) blockMouse = true;

		if (recognizerActive)
		{
			if (IsKeyPressed(KEY_SPACE)) recognizer.Commit();

			if (!recognizer.IsRunning())
			{
				recognizerActive = false;
				if (recognizer.IsComplete())
				{
					if (!cube.isValid())
					{
						backupCubies = cube.cubies;
						showInvalidStateError = true;
						isPaintMode = false;
					}
				}
			}
		}

		handleInput(
			camera,
			blockMouse,
			isPaintMode,
			isSolvingAnim,
			currentPaintColorIdx);

		if (!isSolvingAnim && !cube.isRotating)
		{
			
			if ((!isPaused || stepForward) &&
				!actionQueue.empty())
			{
				lastProcessedMove = actionQueue.front();
				actionQueue.pop_front();

				cube.rotAxis = lastProcessedMove.axis;
				cube.rotLayer = lastProcessedMove.layer;
				cube.rotAngle = 0.0f;
				cube.isRotating = true;

				solveTargetAngle = lastProcessedMove.angle;
				currentSolveAngle = 0.0f;
				isSolvingAnim = true;
				isMovingBack = false;
				stepForward = false;
			}
			
			else if (isPaused && stepBackward && !historyStack.empty())
			{
				lastProcessedMove = historyStack.back();
				historyStack.pop_back();

				cube.rotAxis = lastProcessedMove.axis;
				cube.rotLayer = lastProcessedMove.layer;
				cube.rotAngle = 0.0f;
				cube.isRotating = true;

				solveTargetAngle = -lastProcessedMove.angle; 
				currentSolveAngle = 0.0f;
				isSolvingAnim = true;
				isMovingBack = true;
				stepBackward = false;
			}
		}

		if (isSolvingAnim)
		{
			if (solveTargetAngle < 0)
				 currentSolveAngle -= ANIM_SPEED;
			else currentSolveAngle += ANIM_SPEED;

			if (std::abs(currentSolveAngle) >=
				std::abs(solveTargetAngle))
			{
				cube.rotAngle = solveTargetAngle;
				cube.BakeRotation();
				cube.isRotating = false;
				isSolvingAnim = false;

				if (isMovingBack)
					actionQueue.push_front(lastProcessedMove);
				else
					historyStack.push_back(lastProcessedMove);
			}
			else
				cube.rotAngle = currentSolveAngle;
		}

		auto togglePaintMode
		{
			[&]()
			{
			if (!isPaintMode)
				{
					backupCubies = cube.cubies;
					isPaintMode = true;
				}
				else
				{
					if (cube.isValid()) isPaintMode = false;
					else showInvalidStateError = true;
				}
			}
		};

		if (!recognizerActive && !showInvalidStateError)
		{
			if (isInputtingSteps)
			{
				int key = GetCharPressed();
				while (key > 0)
				{
					if (key >= '0' &&
						key <= '9' &&
						stepInput.length() < 4)
						stepInput += static_cast<char>(key);
					key = GetCharPressed();
				}
				if (IsKeyPressed(KEY_BACKSPACE) &&
					!stepInput.empty()) stepInput.pop_back();
				if (IsKeyPressed(KEY_ENTER)) {
					int steps
					{
						stepInput.empty() ?
						20 : std::stoi(stepInput)
					};
					cube.Shuffle(steps);
					actionQueue.clear();
					historyStack.clear();
					isInputtingSteps = false;
				}
				if (IsKeyPressed(KEY_ESCAPE))
					isInputtingSteps = false;
			}
			else {
				if (IsKeyPressed(KEY_S))
					isInputtingSteps = true;
				if (IsKeyPressed(KEY_BACKSPACE))
				{
					cube.Reset();
					actionQueue.clear();
					historyStack.clear();
				}
				if (IsKeyPressed(KEY_C))
					togglePaintMode();
				if (IsKeyPressed(KEY_V))
				{
					backupCubies = cube.cubies;
					recognizer.Start(&cube); 
					recognizerActive = true;
				}
				if (IsKeyPressed(KEY_P))
					isPaused = true;
				if (IsKeyPressed(KEY_SPACE) && !isQueueActive)
				{
					ResetCameraView(camera); 
					cube.Solve(actionQueue); 
				}
				if (IsKeyPressed(KEY_LEFT_BRACKET))
					stepBackward = true;
				if (IsKeyPressed(KEY_RIGHT_BRACKET))
					stepForward = true;
			}
		}

		BeginDrawing();
		ClearBackground(Color{ 10, 10, 12, 255 });

		BeginMode3D(camera);
		cube.Draw();
		EndMode3D();

		float btnY{ uiPanel.y };
		if (DrawCyberpunkButton(
			{ uiPanel.x, btnY, uiPanel.width, 40 },
			"SHUFFLE (S)", MAGENTA))
			isInputtingSteps = true;
		btnY += 50;
		if (DrawCyberpunkButton(
			{ uiPanel.x, btnY, uiPanel.width, 40 },
			"RESET (Bksp)", GREEN))
		{
			cube.Reset();
			actionQueue.clear();
			historyStack.clear();
		}
		btnY += 50;
		if (DrawCyberpunkButton(
			{ uiPanel.x, btnY, uiPanel.width, 40 },
			"PAINT (C)", SKYBLUE, isPaintMode))
			togglePaintMode();
		btnY += 50;
		if (DrawCyberpunkButton(
			{ uiPanel.x, btnY, uiPanel.width, 40 },
			"RECOGNIZE (V)", ORANGE))
		{
			backupCubies = cube.cubies;
			recognizer.Start(&cube);
			recognizerActive = true;
		}
		btnY += 50;
		if (DrawCyberpunkButton(
			{ uiPanel.x, btnY, uiPanel.width, 40 },
			"SOLVE (Spc)", YELLOW))
		{
			if (!isQueueActive)
			{
				ResetCameraView(camera); 
				cube.Solve(actionQueue); 
			}
		}

		if (isQueueActive)
		{
			Rectangle ctrlPanel
			{
				uiPanel.x,
				uiPanel.y + 350.0f,
				uiPanel.width,
				140.0f
			};
			if (DrawCyberpunkButton(
				{ ctrlPanel.x, ctrlPanel.y, ctrlPanel.width, 40 },
				isPaused ? "RESUME" : "PAUSE",
				SKYBLUE, isPaused))
				isPaused = !isPaused;

			float halfW{ (ctrlPanel.width - 10.0f) / 2.0f };
			if (DrawCyberpunkButton(
				{ ctrlPanel.x, ctrlPanel.y + 50, halfW, 40 },
				"STEP <", ORANGE))
			{
				isPaused = true;
				stepBackward = true;
			}
			if (DrawCyberpunkButton(
				{ ctrlPanel.x + halfW + 10, ctrlPanel.y + 50, halfW, 40 },
				"STEP >", GREEN)) {
				isPaused = true;
				stepForward = true;
			}
		}

		if (isSolvingAnim)
			DrawText(
				isPaused ? "PAUSED" : "PLAYING",
				30, 80, 20,
				isPaused ? YELLOW : GREEN);

		if (recognizerActive)
			DrawText(
				"CHECKING OPENCV WINDOW...",
				30, 30, 20, ORANGE);
		else if (isPaintMode && !showInvalidStateError)
			DrawText(
				"PAINT MODE: Select color from palette. Press 'C' to save.",
				30, 30, 20, SKYBLUE);
		else if (cube.isSolved() && !isPaintMode)
			DrawText("SOLVED!", 30, 30, 40, GREEN);

		if (isPaintMode && !showInvalidStateError)
		{
			DrawRectangleRec(palettePanel, Color{ 20,20,25,200 });
			DrawRectangleLinesEx(palettePanel, 2.0f, SKYBLUE);
			DrawText(
				"SELECT COLOR:",
				(int)palettePanel.x + 10,
				(int)palettePanel.y + 10,
				10, GRAY);
			for (int i{ 0 }; i < 6; ++i)
			{
				Rectangle colorBtn
				{
					palettePanel.x + 20.0f + i * 48.0f,
					palettePanel.y + 25.0f,
					40.0f,
					40.0f
				};
				DrawRectangleRec(colorBtn, FACE_COLORS[i]);
				if (i == currentPaintColorIdx)
				{
					DrawRectangleLinesEx(colorBtn, 3.0f, WHITE);
					DrawRectangleLinesEx(
						{
							colorBtn.x - 2,
							colorBtn.y - 2,
							colorBtn.width + 4,
							colorBtn.height + 4
						}
					, 1.0f, SKYBLUE);
				}
				else DrawRectangleLinesEx(colorBtn, 1.0f, DARKGRAY);
				if (CheckCollisionPointRec(GetMousePosition(), colorBtn) &&
					IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
					currentPaintColorIdx = i;
			}
		}

		if (showInvalidStateError)
		{
			DrawRectangle(
				0, 0,
				SCREEN_WIDTH, SCREEN_HEIGHT,
				Color{ 0,0,0,180 });
			Rectangle box
			{
				SCREEN_CENTER_X - 220.0f,
				SCREEN_CENTER_Y - 70.0f,
				440.0f,
				140.0f
			};
			DrawRectangleRec(box, Color{ 20,20,25,255 });
			DrawRectangleLinesEx(box, 2.0f, RED);
			DrawText(
				"INVALID CUBE STATE!",
				(int)box.x + 105,
				(int)box.y + 25,
				20, RED);
			DrawText(
				"Confirm to restore, Cancel to keep current state.",
				(int)box.x + 25,
				(int)box.y + 55,
				16, GRAY);
			if (DrawCyberpunkButton(
				{ box.x + 50, box.y + 85, 150, 35 },
				"CONFIRM", RED))
			{
				cube.cubies = backupCubies;
				showInvalidStateError = false;
				isPaintMode = false;
			}
			if (DrawCyberpunkButton(
				{ box.x + 240, box.y + 85, 150, 35 },
				"CANCEL", GRAY)) showInvalidStateError = false;
		}

		if (isInputtingSteps)
		{
			DrawRectangle(
				0, 0,
				SCREEN_WIDTH, SCREEN_HEIGHT,
				Color{ 0,0,0,180 });
			Rectangle box
			{
				SCREEN_CENTER_X - 150.0f,
				SCREEN_CENTER_Y - 80.0f,
				300.0f,
				170.0f
			};
			DrawRectangleRec(box, Color{ 20,20,25,255 });
			DrawRectangleLinesEx(box, 2.0f, MAGENTA);
			DrawText(
				"ENTER SHUFFLE STEPS:",
				(int)box.x + 20,
				(int)box.y + 20,
				20, SKYBLUE);
			Rectangle inputBox
			{
				box.x + 20.0f,
				box.y + 50.0f,
				box.width - 40.0f,
				40.0f
			};
			DrawRectangleRec(inputBox, Color{ 10,10,12,255 });
			DrawRectangleLinesEx(inputBox, 1.0f, WHITE);
			DrawText(
				stepInput.c_str(),
				(int)inputBox.x + 10,
				(int)inputBox.y + 10,
				20, WHITE);
			if ((static_cast<int>(GetTime() * 2) % 2) == 0)
			{
				int textW{ MeasureText(stepInput.c_str(), 20) };
				DrawRectangle(
					(int)inputBox.x + 12 + textW,
					(int)inputBox.y + 10,
					10, 20, MAGENTA);
			}
			if (DrawCyberpunkButton(
				{ box.x + 20, box.y + 110, 120, 40 },
				"CONFIRM", MAGENTA))
			{
				int steps
				{
					stepInput.empty() ?
					20 : std::stoi(stepInput)
				};
				cube.Shuffle(steps);
				actionQueue.clear();
				historyStack.clear();
				isInputtingSteps = false;
			}
			if (DrawCyberpunkButton(
				{ box.x + 160, box.y + 110, 120, 40 },
				"CANCEL", GRAY)) isInputtingSteps = false;
		}

		EndDrawing();
	}

	cube.SaveState("cube_state.bin");
	CloseWindow();
	return 0;
}