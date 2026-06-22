#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/stat.h>

static std::atomic<bool> g_running{true};
static void sigHandler(int) { g_running = false; }

namespace {

struct AppConfig {
    int cameraIndex = -1;
    int width = 640;
    int height = 480;
    double motionThreshold = 0.018;
    std::string sourceUrl;
    std::string streamBaseUrl = "http://192.168.45.33:5000/video_feed/";
};

struct CameraMode {
    int width;
    int height;
    double fps;
    int fourcc;
    const char* name;
};

const std::vector<CameraMode> kCameraModes = {
    {320,  240, 15, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "320x240 MJPG 15fps"},
    {320,  240, 15, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'), "320x240 YUYV 15fps"},
    {640,  480, 15, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "640x480 MJPG 15fps"},
    {640,  480, 15, cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V'), "640x480 YUYV 15fps"},
    {1280, 720, 15, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "1280x720 MJPG 15fps"},
    {1280, 720, 30, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "1280x720 MJPG 30fps"}
};

bool videoDeviceExists(int index) {
    const std::string path = "/dev/video" + std::to_string(index);
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISCHR(info.st_mode);
}

std::vector<int> cameraCandidates(const AppConfig& config) {
    if (config.cameraIndex >= 0) {
        return {config.cameraIndex};
    }

    std::vector<int> candidates;
    for (int index = 0; index <= 63; ++index) {
        if (videoDeviceExists(index)) {
            candidates.push_back(index);
        }
    }
    return candidates;
}

std::string videoDevicePath(int index) {
    return "/dev/video" + std::to_string(index);
}

bool looksLikeStreamUrl(const std::string& value) {
    return value.rfind("http://", 0) == 0 ||
           value.rfind("https://", 0) == 0 ||
           value.rfind("rtsp://", 0) == 0;
}

bool parseNonNegativeInt(const std::string& value, int& number) {
    try {
        size_t parsed = 0;
        const int parsedNumber = std::stoi(value, &parsed);
        if (parsed != value.size() || parsedNumber < 0) {
            return false;
        }
        number = parsedNumber;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void configureSourceFromInput(AppConfig& config, const std::string& input, bool preferStreamIndex) {
    if (input.empty() || input == "stream") {
        config.sourceUrl = config.streamBaseUrl + "1";
        return;
    }
    if (looksLikeStreamUrl(input)) {
        config.sourceUrl = input;
        return;
    }
    if (input == "auto") {
        return;
    }

    int index = -1;
    if (parseNonNegativeInt(input, index)) {
        if (preferStreamIndex) {
            config.sourceUrl = config.streamBaseUrl + std::to_string(index);
        } else {
            config.cameraIndex = index;
        }
        return;
    }

    throw std::invalid_argument("invalid camera source");
}

bool promptForCameraSource(AppConfig& config) {
    std::cout
        << "Camera source input\n"
        << "  0,1,2.. : Windows stream camera ID\n"
        << "  stream  : Windows stream camera ID 1\n"
        << "  auto    : local /dev/video* auto detection\n"
        << "  URL     : http://... or rtsp://...\n"
        << "Select source [stream]: "
        << std::flush;

    std::string input;
    if (!std::getline(std::cin, input)) {
        return false;
    }

    try {
        configureSourceFromInput(config, input, true);
    } catch (const std::exception&) {
        std::cerr << "Invalid camera source: " << input << "\n";
        return false;
    }

    return true;
}

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

bool isWindowOpen(const std::string& windowName) {
    try {
        return cv::getWindowProperty(windowName, cv::WND_PROP_VISIBLE) >= 1;
    } catch (const cv::Exception&) {
        return false;
    }
}

bool openCamera(cv::VideoCapture& camera, AppConfig& config, std::string& backendName) {
    if (!config.sourceUrl.empty()) {
        std::cout << "Opening stream URL " << config.sourceUrl << "...\n";
        camera.open(config.sourceUrl, cv::CAP_ANY);
        if (!camera.isOpened()) {
            return false;
        }

        camera.set(cv::CAP_PROP_BUFFERSIZE, 1);

        cv::Mat probeFrame;
        camera.read(probeFrame);
        if (probeFrame.empty()) {
            std::cerr << "Stream opened but returned no frame.\n";
            camera.release();
            return false;
        }

        config.width = probeFrame.cols;
        config.height = probeFrame.rows;
        backendName = "stream URL";
        return true;
    }

    for (const int index : cameraCandidates(config)) {
        const std::string devicePath = videoDevicePath(index);
        for (const auto& mode : kCameraModes) {
            std::cout << "Opening " << devicePath
                      << " with V4L2"
                      << " (" << mode.name << ")...\n";

            camera.open(devicePath, cv::CAP_V4L2);
            if (!camera.isOpened()) {
                camera.release();
                continue;
            }

            camera.set(cv::CAP_PROP_FOURCC, mode.fourcc);
            camera.set(cv::CAP_PROP_FRAME_WIDTH, mode.width);
            camera.set(cv::CAP_PROP_FRAME_HEIGHT, mode.height);
            camera.set(cv::CAP_PROP_FPS, mode.fps);
            camera.set(cv::CAP_PROP_BUFFERSIZE, 1);

            cv::Mat probeFrame;
            camera.read(probeFrame);

            if (probeFrame.empty()) {
                std::cerr << devicePath << " opened with " << mode.name
                          << " but returned no frame.\n";
                camera.release();
                continue;
            }

            config.cameraIndex = index;
            config.width = probeFrame.cols;
            config.height = probeFrame.rows;
            backendName = std::string("V4L2 ") + mode.name;
            return true;
        }
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
        const std::string cameraArg = argv[1];
        try {
            configureSourceFromInput(config, cameraArg, false);
        } catch (const std::exception&) {
            std::cerr << "Invalid camera source: " << cameraArg << "\n";
            std::cerr << "Use a numeric index like 0, auto, stream, or a URL like http://host:port/video\n";
            return 1;
        }
    } else if (!promptForCameraSource(config)) {
        return 1;
    }

    cv::VideoCapture camera;
    std::string backendName;
    if (!openCamera(camera, config, backendName)) {
        logLine("failed to open camera");
        if (config.cameraIndex >= 0) {
            std::cerr << "Failed to open camera index " << config.cameraIndex << ".\n";
            std::cerr << "Try auto detection: ./CameraApp auto\n";
        } else if (!config.sourceUrl.empty()) {
            std::cerr << "Failed to open stream URL: " << config.sourceUrl << "\n";
            std::cerr << "Check that the Windows streaming app is running and reachable from WSL.\n";
        } else {
            std::cerr << "Failed to open any camera index.\n";
            std::cerr << "Try a specific index, for example: ./CameraApp 0\n";
        }
        std::cerr << "Check /dev/video* devices and usbipd-win attachment.\n";
        return 1;
    }
    logLine("camera opened with " + backendName);

    printHelp();
    std::cout << "Input size: " << config.width << "x" << config.height
              << "  source: "
              << (config.sourceUrl.empty() ? videoDevicePath(config.cameraIndex) : config.sourceUrl)
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

        const int key = cv::waitKey(10);
        if (!isWindowOpen(windowName)) {
            g_running = false;
            break;
        }
        if (key == 27 || key == 'q' || key == 'Q') {
            g_running = false;
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

    camera.release();
    cv::destroyAllWindows();

    return 0;
}
