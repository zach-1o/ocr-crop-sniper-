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
#include <atomic>
#include <vector>

using json = nlohmann::json;

// =======================================================
// ---------------- SYSTEM STATE --------------------------
// =======================================================

enum class SystemState {
    RUNNING,
    HALTED
};

std::atomic<SystemState> system_state(SystemState::RUNNING);
std::atomic<int> target_index(-1);
std::atomic<bool> ui_busy(false);

// =======================================================
// ---------------- HELPERS --------------------------------
// =======================================================

int parse_int(const char* txt) {
    if (!txt) return -1;
    std::string s(txt);
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    if (s.empty()) return -1;
    for (char c : s)
        if (!isdigit(c)) return -1;
    return std::stoi(s);
}

// ---------------- Mouse Click (FAST) --------------------

void mouse_click(int x, int y, int screenW, int screenH) {
    INPUT input{};

    input.type = INPUT_MOUSE;
    input.mi.dx = x * 65535 / screenW;
    input.mi.dy = y * 65535 / screenH;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));

    ZeroMemory(&input, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));

    ZeroMemory(&input, sizeof(INPUT));
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

// =======================================================
// ---------------- SCREEN CAPTURE ------------------------
// =======================================================

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

// =======================================================
// ---------------- MAIN ----------------------------------
// =======================================================

int main() {
    SetProcessDPIAware();

    std::cout << "SCAN STARTED\n";

    // ---------- Load calibration ----------
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
    auto buy_buttons = cfg["buy_buttons"];

    POINT tab_default{
        cfg["tabs"]["default"]["x"],
        cfg["tabs"]["default"]["y"]
    };

    POINT tab_large{
        cfg["tabs"]["large"]["x"],
        cfg["tabs"]["large"]["y"]
    };

    int topY = INT_MAX, bottomY = 0;
    for (auto& o : offers) {
        topY = std::min(topY, (int)o["y1"]);
        bottomY = std::max(bottomY, (int)o["y2"]);
    }

    int stripW = x2 - x1;

    // ---------- Init Tesseract ----------
    tesseract::TessBaseAPI tess;
    tess.Init("./tessdata", "eng", tesseract::OEM_LSTM_ONLY);
    tess.SetPageSegMode(tesseract::PSM_SINGLE_LINE);
    tess.SetVariable("tessedit_char_whitelist", "0123456789");
    tess.SetVariable("load_system_dawg", "0");
    tess.SetVariable("load_freq_dawg", "0");

    std::cout << "TESSERACT INITIALIZED\n\n";

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // ===================================================
    // ---------------- VISION THREAD --------------------
    // ===================================================

    std::thread vision([&]() {
        while (system_state == SystemState::RUNNING) {

            if (GetAsyncKeyState(VK_F12) & 1) system_state = SystemState::HALTED;

            if (ui_busy) {
                Sleep(1);
                continue;
            }

            auto start = std::chrono::high_resolution_clock::now();

            cv::Mat screen = capture_screen();
            cv::Mat strip = screen(
                cv::Range(topY, bottomY),
                cv::Range(x1, x2)
            );

            static cv::Mat last;
            if (!last.empty()) {
                cv::Mat diff;
                cv::absdiff(strip, last, diff);
                if (cv::mean(diff)[0] > 15) {
                    last = strip.clone();
                    continue;
                }
            }
            last = strip.clone();

            static int log_counter = 0;
            bool do_log = (++log_counter % 10 == 0);

            if (do_log) std::cout << "Offers: ";

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

                if (cv::mean(gray)[0] > 245 || cv::mean(gray)[0] < 10) {
                     std::cout << "-- ";
                     continue;
                }

                tess.SetImage(gray.data, gray.cols, gray.rows, 1, gray.step);
                char* txt = tess.GetUTF8Text();
                int val = parse_int(txt);
                delete[] txt;

                if (val >= 0) {
                    if (do_log) std::cout << val << " ";
                    if (val == 1000) {
                        target_index = i;
                        std::cout << "[TARGET] ";
                        return; // STOP vision immediately
                    }
                } else {
                    if (do_log) std::cout << "-- ";
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            if (do_log) std::cout << "| Scan time: " << ms << " ms\n";
        }
    });

    // ===================================================
    // ---------------- ACTION THREAD --------------------
    // ===================================================

    std::thread action([&]() {
        while (system_state == SystemState::RUNNING) {
            if (GetAsyncKeyState(VK_F12) & 1) system_state = SystemState::HALTED;
            int idx = target_index.load();
            if (idx >= 0) {
                mouse_click(
                    buy_buttons[idx]["x"],
                    buy_buttons[idx]["y"],
                    screenW,
                    screenH
                );
                system_state = SystemState::HALTED;
                return;
            }
            Sleep(1);
        }
    });

    // ===================================================
    // ---------------- TAB THREAD -----------------------
    // ===================================================

    std::thread tabs([&]() {
        while (system_state == SystemState::RUNNING) {
            if (GetAsyncKeyState(VK_F12) & 1) system_state = SystemState::HALTED;
            ui_busy = true;
            mouse_click(tab_default.x, tab_default.y, screenW, screenH);
            Sleep(200);
            ui_busy = false;

            Sleep(800);

            if (system_state != SystemState::RUNNING) return;

            ui_busy = true;
            mouse_click(tab_large.x, tab_large.y, screenW, screenH);
            Sleep(200);
            ui_busy = false;

            Sleep(800);
        }
    });

    // ===================================================
    // ---------------- JOIN -----------------------------
    // ===================================================

    vision.join();
    action.join();
    tabs.join();

    std::cout << "\nSYSTEM HALTED\n";
    return 0;
}
