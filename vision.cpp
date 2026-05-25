#include "vision.h"
#include <fstream>
#include <cmath>
#include <string>
#include "config.h"

double GetHSVDistance(const cv::Scalar& c1, const cv::Scalar& c2) {
	double dh{ std::abs(c1[0] - c2[0]) };
	if (dh > 90.0) dh = 180.0 - dh;
	return dh * 2.5 + std::abs(c1[1] - c2[1]) * 0.5 + std::abs(c1[2] - c2[2]) * 0.5;
}

constexpr int GeoToColorIdx[6]{ 0, 2, 4, 1, 3, 5 };
const char* ColorNames[6]{ "Yellow", "Red", "Blue", "White", "Orange", "Green" };
const char* ColorLetters[6]{ "Y", "R", "B", "W", "O", "G" };
const cv::Scalar OpenCVColors[6]
{
	{0, 255, 255},   
	{0, 0, 255},     
	{255, 0, 0},     
	{255, 255, 255}, 
	{0, 165, 255},   
	{0, 255, 0}      
};

Recognizer::Recognizer() {}
Recognizer::~Recognizer() { Cancel(); }

void Recognizer::Start(Cube* cube) {
	if (running) return;
	if (captureThread.joinable())captureThread.join();
	targetCube = cube;
	done = false;
	commitRequested = false;
	faceScanned.fill(false);

	std::ifstream in{ "calibration.bin", std::ios::binary };
	if (in) {
		in.read(reinterpret_cast<char*>(calibratedHSV.data()), sizeof(cv::Scalar) * 6);
		isCalibrating = false;
	}
	else {
		isCalibrating = true;
		calibStep = 0;
	}

	running = true;
	captureThread = std::thread(&Recognizer::CameraLoop, this);
}

void Recognizer::Commit() { commitRequested = true; }
void Recognizer::Cancel() {
	running = false;
	if (captureThread.joinable()) captureThread.join();
}

void Recognizer::CameraLoop() {
	cv::VideoCapture cap{ "http://10.2.120.128:8080/video" };
	if (!cap.isOpened()) { running = false; return; }

	cv::namedWindow("Scanner Feed", cv::WINDOW_AUTOSIZE);

	cv::Mat frame, hsv, displayFrame;
	while (running) {
		cap >> frame;
		if (frame.empty()) continue;

		frame.copyTo(displayFrame);
		cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

		int cx{ frame.cols / 2 };
		int cy{ frame.rows / 2 };
		int offset{ 90 };

		if (isCalibrating) {
			
			cv::Rect roi{ cx - 25, cy - 25, 50, 50 };
			roi &= cv::Rect{ 0, 0, hsv.cols, hsv.rows };

			if (roi.area() > 0) {
				cv::rectangle(displayFrame, roi, OpenCVColors[calibStep], 3);
				cv::Scalar avg{ cv::mean(hsv(roi)) };

				
				cv::putText(displayFrame, std::string("Calibrating: ") + ColorNames[calibStep],
					cv::Point{ 20, 30 }, cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar{ 0, 255, 255 }, 2);
				cv::putText(displayFrame, "Point to this center color and press SPACE",
					cv::Point{ 20, 70 }, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar{ 255, 255, 255 }, 2);

				
				int baseline{ 0 };
				cv::Size textSize{ cv::getTextSize(ColorLetters[calibStep], cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline) };
				cv::Point textPos{ cx - textSize.width / 2, cy + textSize.height / 2 };
				cv::putText(displayFrame, ColorLetters[calibStep], textPos + cv::Point{ 2, 2 },
					cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar{ 0, 0, 0 }, 2);
				cv::putText(displayFrame, ColorLetters[calibStep], textPos,
					cv::FONT_HERSHEY_SIMPLEX, 1.0, OpenCVColors[calibStep], 2);

				if (commitRequested) {
					commitRequested = false;
					calibratedHSV[calibStep] = avg;
					calibStep++;
					if (calibStep >= 6) {
						std::ofstream out{ "calibration.bin", std::ios::binary };
						out.write(reinterpret_cast<const char*>(calibratedHSV.data()), sizeof(cv::Scalar) * 6);
						isCalibrating = false;
					}
				}
			}
		}
		else {
			std::array<int, 9> colors{};
			for (int i{ 0 }; i < 3; ++i) {
				for (int j{ 0 }; j < 3; ++j) {
					
					int x{ cx + (j - 1) * offset - 25 };
					int y{ cy + (i - 1) * offset - 25 };
					cv::Rect roi{ x, y, 50, 50 };
					roi &= cv::Rect{ 0, 0, hsv.cols, hsv.rows };

					if (roi.area() == 0) continue;
					cv::Scalar avg{ cv::mean(hsv(roi)) };

					double minDist{ 1e9 };
					int best{ 0 };
					for (int c{ 0 }; c < 6; ++c) {
						double d{ GetHSVDistance(avg, calibratedHSV[c]) };
						if (d < minDist) { minDist = d; best = c; }
					}
					colors[i * 3 + j] = best;

					cv::rectangle(displayFrame, roi, OpenCVColors[best], 3);

					
					int baseline{ 0 };
					cv::Size textSize{ cv::getTextSize(ColorLetters[best], cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline) };
					cv::Point textPos{
						roi.x + roi.width / 2 - textSize.width / 2,
						roi.y + roi.height / 2 + textSize.height / 2
					};
					cv::putText(displayFrame, ColorLetters[best], textPos + cv::Point{ 2, 2 },
						cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar{ 0, 0, 0 }, 2);
					cv::putText(displayFrame, ColorLetters[best], textPos,
						cv::FONT_HERSHEY_SIMPLEX, 1.0, OpenCVColors[best], 2);
				}
			}

			int detectedGeo{ colors[4] }; 

			
			cv::putText(displayFrame, std::string("Detected Center: ") + ColorNames[detectedGeo],
				cv::Point{ 20, 30 }, cv::FONT_HERSHEY_SIMPLEX, 1.0, OpenCVColors[detectedGeo], 2);

			
			for (int i{ 0 }; i < 6; ++i) {
				cv::Scalar statusColor{ faceScanned[i] ? cv::Scalar{0, 255, 0} : cv::Scalar{0, 0, 255} };
				cv::putText(displayFrame, std::string(ColorNames[i]) + ": " + (faceScanned[i] ? "OK" : "WAIT"),
					cv::Point{ 20, 70 + i * 30 }, cv::FONT_HERSHEY_SIMPLEX, 0.7, statusColor, 2);
			}

			cv::putText(displayFrame, "Press 'C' to Recalibrate",
				cv::Point{ 20, hsv.rows - 30 }, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar{ 0, 255, 255 }, 2);

			if (commitRequested) {
				commitRequested = false;
				if (targetCube) {
					std::lock_guard<std::mutex> lock{ stateMutex };
					int colorIdx{ GeoToColorIdx[detectedGeo] };
					for (int j{ 0 }; j < 3; ++j) {
						for (int k{ 0 }; k < 3; ++k) {
							int x{ std::array<int, 6>{ k,     2,     k,     k,     0, 2 - k }[detectedGeo] };
							int y{ std::array<int, 6>{ 2, 2 - j, 2 - j,     0, 2 - j, 2 - j }[detectedGeo] };
							int z{ std::array<int, 6>{ j, 2 - k,     2, 2 - j,     k,     0 }[detectedGeo] };

							int matchedGeo{ colors[j * 3 + k] };
							targetCube->cubies[x][y][z].colors[colorIdx] = FACE_COLORS[GeoToColorIdx[matchedGeo]];
							targetCube->cubies[x][y][z].dirty = true;
						}
					}
					faceScanned[detectedGeo] = true;

					bool allDone{ true };
					for (bool s : faceScanned) if (!s) allDone = false;
					if (allDone) {
						running = false;
						done = true;
					}
				}
			}
		}

		cv::imshow("Scanner Feed", displayFrame);

		int key{ cv::waitKey(1) & 0xFF };
		if (key == 'c' || key == 'C') {
			isCalibrating = true;
			calibStep = 0;
			faceScanned.fill(false);
		}
		else if (key == ' ') {
			commitRequested = true;
		}
		else if (key == 27) {
			running = false;
		}
	}

	cv::destroyWindow("Scanner Feed");
}