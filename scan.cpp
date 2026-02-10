#include <opencv2/opencv.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <nlohmann/json.hpp>
#include <windows.h>

#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

using json = nlohmann::json;

// ---------- helper ----------
int parse_int(const char* txt) {
    if (!txt) return -1;
    std::string s(txt);
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    if (s.empty()) return -1;
    for (char c : s)
        if (!isdigit(c)) return -1;
    return std::stoi(s);
}

// ---------- full screen capture ----------
cv::Mat capture_screen() {
    HWND hwnd = GetDesktopWindow();
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
    SelectObject(hdcMem, hBitmap);

    BitBlt(hdcMem, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = w;
    bi.biHeight = -h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    cv::Mat img(h, w, CV_8UC3);
    GetDIBits(hdcMem, hBitmap, 0, h, img.data,
              (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcScreen);
    return img;
}

int main() {
    SetProcessDPIAware();

    std::cout << "SCAN STARTED\n";

    // ---------- load calibration ----------
    json cfg;
    std::ifstream f("calibration.json");
    if (!f) {
        std::cerr << "calibration.json not found\n";
        return 1;
    }
    f >> cfg;

    int x1 = cfg["x1"];
    int x2 = cfg["x2"];
    auto offers = cfg["offers"];

    int topY = INT_MAX, bottomY = 0;
    for (auto& o : offers) {
        topY = std::min(topY, (int)o["y1"]);
        bottomY = std::max(bottomY, (int)o["y2"]);
    }

    int stripW = x2 - x1;
    int stripH = bottomY - topY;

    // ---------- init tesseract ----------
    tesseract::TessBaseAPI tess;
    tess.Init("./tessdata", "eng", tesseract::OEM_LSTM_ONLY);
    tess.SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    tess.SetVariable("tessedit_char_whitelist", "0123456789");
    tess.SetVariable("load_system_dawg", "0");
    tess.SetVariable("load_freq_dawg", "0");

    std::cout << "TESSERACT INITIALIZED\n\n";

    // ---------- scan loop ----------
    while (true) {
        auto start = std::chrono::high_resolution_clock::now();

        // 1️⃣ Full screen capture (GPU-safe)
        cv::Mat screen = capture_screen();

        // 2️⃣ Crop vertical strip (cheap)
        cv::Mat strip = screen(
            cv::Range(topY, bottomY),
            cv::Range(x1, x2)
        );

        std::cout << "Offers: ";

        // 3️⃣ Crop 7 offers
        for (int i = 0; i < 7; i++) {
            int y1 = offers[i]["y1"].get<int>() - topY;
            int y2 = offers[i]["y2"].get<int>() - topY;

            cv::Mat roi = strip(
                cv::Range(y1, y2),
                cv::Range(0, stripW)
            );

            cv::Mat gray;
            cv::cvtColor(roi, gray, cv::COLOR_BGR2GRAY);
            cv::convertScaleAbs(gray, gray, 1.3, 0);

            tess.SetImage(gray.data, gray.cols, gray.rows, 1, gray.step);
            char* txt = tess.GetUTF8Text();
            int val = parse_int(txt);
            delete[] txt;

            if (val >= 0) std::cout << val << " ";
            else std::cout << "-- ";
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "| Scan time: " << ms << " ms\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}
