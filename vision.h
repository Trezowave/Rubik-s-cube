#pragma once
#include <opencv2/opencv.hpp>
#include "cube.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <array>

class Recognizer {
public:
	Recognizer();
	~Recognizer();

	void Start(Cube* cube);
	void Commit();
	void Cancel();

	bool IsRunning() const { return running; }
	bool IsComplete() const { return done; }

private:
	void CameraLoop();

	std::thread captureThread;
	std::mutex stateMutex;

	std::atomic<bool> running{ false };
	std::atomic<bool> done{ false };
	std::atomic<bool> commitRequested{ false };

	bool isCalibrating{ false };
	int calibStep{ 0 };

	std::array<cv::Scalar, 6> calibratedHSV{};
	std::array<bool, 6> faceScanned{};

	Cube* targetCube{ nullptr };
};