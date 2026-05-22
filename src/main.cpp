#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

namespace {

struct AppConfig {
    int cameraIndex = 48;
    int width = 1280;
    int height = 720;
    double motionThreshold = 0.018;
};

struct CameraBackend {
    int api;
    const char* name;
};

const std::vector<CameraBackend> kCameraBackends = {
    {cv::CAP_V4L2, "V4L2"},
    {cv::CAP_ANY,  "Auto"}
};

void logLine(const std::string& text) {
    std::ofstream log("camera_wsl.log", std::ios::app);
    log << text << '\n';
}

std::string makeStatusText(double fps, const cv::Size& size, bool motionEnabled, double motionRatio) {
    std::ostringstream out;
    out << "FPS " << std::fixed << std::setprecision(1) << fps
        << " | " << size.width << "x" << size.height
        << " | motion " << (motionEnabled ? "on" : "off");

    if (motionEnabled) {
        out << " (" << std::setprecision(2) << motionRatio * 100.0 << "%)";
    }

    return out.str();
}

void drawLabel(cv::Mat& frame, const std::string& text, cv::Point origin,
               double scale = 0.7, cv::Scalar color = {255, 255, 255}) {
    int baseline = 0;
    const int thickness = 2;
    const auto textSize = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, scale, thickness, &baseline);
    const cv::Rect background(origin.x - 8, origin.y - textSize.height - 8,
                              textSize.width + 16, textSize.height + baseline + 14);

    cv::rectangle(frame, background, {20, 20, 20}, cv::FILLED);
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_SIMPLEX, scale, color, thickness, cv::LINE_AA);
}

double updateMotionMask(const cv::Mat& frame, cv::Mat& previousGray, cv::Mat& motionMask) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {21, 21}, 0);

    if (previousGray.empty()) {
        previousGray = gray;
        motionMask = cv::Mat::zeros(frame.size(), CV_8UC1);
        return 0.0;
    }

    cv::Mat delta;
    cv::absdiff(previousGray, gray, delta);
    cv::threshold(delta, motionMask, 25, 255, cv::THRESH_BINARY);
    cv::dilate(motionMask, motionMask, {}, {-1, -1}, 2);

    previousGray = gray;
    return static_cast<double>(cv::countNonZero(motionMask)) / static_cast<double>(motionMask.total());
}

void drawMotionOverlay(cv::Mat& frame, const cv::Mat& motionMask, double motionRatio, double threshold) {
    cv::Mat redOverlay(frame.size(), frame.type(), {0, 0, 255});
    redOverlay.copyTo(frame, motionMask);

    const bool alert = motionRatio >= threshold;
    const auto color = alert ? cv::Scalar{0, 0, 255} : cv::Scalar{0, 200, 0};
    const std::string state = alert ? "MOTION DETECTED" : "Monitoring";
    drawLabel(frame, state, {20, 72}, 0.85, color);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(motionMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& contour : contours) {
        if (cv::contourArea(contour) < 900.0) {
            continue;
        }
        cv::rectangle(frame, cv::boundingRect(contour), color, 2);
    }
}

void printHelp() {
    std::cout
        << "CameraWSLVision controls\n"
        << "  q / ESC : exit\n"
        << "  m       : toggle motion detection overlay\n"
        << "  s       : save current frame as capture.png\n";
}

bool openCamera(cv::VideoCapture& camera, const AppConfig& config, std::string& backendName) {
    for (const auto& backend : kCameraBackends) {
        std::cout << "Opening camera index " << config.cameraIndex
                  << " with " << backend.name << "...\n";

        camera.open(config.cameraIndex, backend.api);
        if (!camera.isOpened()) {
            camera.release();
            continue;
        }

        camera.set(cv::CAP_PROP_FRAME_WIDTH, config.width);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, config.height);
        camera.set(cv::CAP_PROP_FPS, 30);
        camera.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        camera.set(cv::CAP_PROP_BUFFERSIZE, 1);

        backendName = backend.name;
        return true;
    }

    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    logLine("camera_wsl start");
    AppConfig config;
    if (argc > 1) {
        config.cameraIndex = std::stoi(argv[1]);
    }

    cv::VideoCapture camera;
    std::string backendName;
    if (!openCamera(camera, config, backendName)) {
        logLine("failed to open camera");
        std::cerr << "Failed to open camera index " << config.cameraIndex << ".\n";
        std::cerr << "Try another index, for example: ./CameraApp 1\n";
        std::cerr << "Check /dev/video* devices and usbipd-win attachment.\n";
        return 1;
    }
    logLine("camera opened with " + backendName);

    printHelp();
    std::cout << "Format: MJPG  requested size: " << config.width << "x" << config.height
              << "  camera index: " << config.cameraIndex
              << "  backend: " << backendName << "\n";

    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if ((display == nullptr || display[0] == '\0') &&
        (wayland == nullptr || wayland[0] == '\0')) {
        std::cerr << "경고: DISPLAY/WAYLAND_DISPLAY가 없어 창을 표시할 수 없습니다.\n";
        std::cerr << "WSLg가 활성화된 세션에서 실행하세요.\n";
    }

    const std::string windowName = "CameraWSLVision";
    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    bool motionEnabled = true;
    cv::Mat frame;
    cv::Mat previousGray;
    cv::Mat motionMask;
    double motionRatio = 0.0;
    double fps = 0.0;
    int frameCount = 0;
    int failedReads = 0;
    const auto start = std::chrono::steady_clock::now();
    auto lastTick = std::chrono::steady_clock::now();

    while (g_running) {
        if (!camera.read(frame) || frame.empty()) {
            ++failedReads;
            if (failedReads % 100 == 0) {
                std::cerr << "Camera frame read failures continue. count=" << failedReads << "\n";
            }

            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (frameCount == 0 && elapsed > std::chrono::seconds(5)) {
                std::cerr << "No first frame received for 5 seconds.\n";
                std::cerr << "Check usbipd-win attachment or whether a Windows app is using the camera.\n";
                break;
            }
            continue;
        }

        if (frameCount == 0) {
            std::cout << "First frame received: " << frame.cols << "x" << frame.rows
                      << "  type=" << frame.type()
                      << "  mean=" << cv::mean(frame)[0] << "\n";
        }
        ++frameCount;

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;
        if (elapsed > 0.0) {
            fps = 0.9 * fps + 0.1 * (1.0 / elapsed);
        }

        if (motionEnabled) {
            motionRatio = updateMotionMask(frame, previousGray, motionMask);
            drawMotionOverlay(frame, motionMask, motionRatio, config.motionThreshold);
        } else {
            previousGray.release();
            motionRatio = 0.0;
        }

        drawLabel(frame, makeStatusText(fps, frame.size(), motionEnabled, motionRatio), {20, 32});
        cv::imshow(windowName, frame);

        const int key = cv::waitKey(1);
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }
        if (key == 'm' || key == 'M') {
            motionEnabled = !motionEnabled;
        }
        if (key == 's' || key == 'S') {
            cv::imwrite("capture.png", frame);
            std::cout << "Saved capture.png\n";
        }
    }

    return 0;
}
