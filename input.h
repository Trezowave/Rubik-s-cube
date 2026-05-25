#pragma once
#include <raylib.h>

void handleInput(
	Camera& camera,
	bool blockMouse,
	bool isPaintMode,
	bool isSolvingAnim,
	int paintColorIdx);

void ResetCameraView(Camera& camera);