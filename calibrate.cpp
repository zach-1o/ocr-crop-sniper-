#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

cv::Mat img;
int step = 0;
int x1 = -1, x2 = -1;
std::vector<int> y_points;

void mouse_cb(int event, int x, int y, int, void*) {
    if (event != cv::EVENT_LBUTTONDOWN) return;

    if (step == 0) {
        x1 = x;
        step = 1;
        std::cout << "x1 set: " << x1 << std::endl;
    }
    else if (step == 1) {
        x2 = x;
        step = 2;
        std::cout << "x2 set: " << x2 << std::endl;
    }
    else {
        y_points.push_back(y);
        std::cout << "y set: " << y << std::endl;
    }
}

int main() {
    img = cv::imread("screenshot.png");
    if (img.empty()) {
        std::cerr << "❌ Failed to load screenshot.png\n";
        std::cerr << "Press ENTER to exit...\n";
        std::cin.get();
        return 1;
    }

    cv::namedWindow("calibrate");
    cv::setMouseCallback("calibrate", mouse_cb);

    while (true) {
        cv::Mat vis = img.clone();

        if (x1 != -1)
            cv::line(vis, {x1,0}, {x1,vis.rows}, {0,255,0}, 2);
        if (x2 != -1)
            cv::line(vis, {x2,0}, {x2,vis.rows}, {0,255,0}, 2);

        for (int y : y_points)
            cv::line(vis, {0,y}, {vis.cols,y}, {255,0,0}, 2);

        cv::imshow("calibrate", vis);
        int k = cv::waitKey(0);  // BLOCK until key press

        if (k == 13 && y_points.size() == 14) break; // ENTER
        if (k == 27) return 0; // ESC
    }

    json j;
    j["x1"] = std::min(x1, x2);
    j["x2"] = std::max(x1, x2);
    j["offers"] = json::array();

    for (int i = 0; i < 14; i += 2) {
        int y1 = std::min(y_points[i], y_points[i+1]);
        int y2 = std::max(y_points[i], y_points[i+1]);
        j["offers"].push_back({{"y1", y1}, {"y2", y2}});
    }

    std::ofstream f("calibration.json");
    f << j.dump(4);
    f.close();

    std::cout << "Saved calibration.json\n";
    return 0;
}
