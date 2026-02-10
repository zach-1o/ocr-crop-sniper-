#!/bin/bash
g++ scan.cpp -o scan.exe \
    -std=c++17 \
    $(pkg-config --cflags opencv4) \
    -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
    -lgdi32 -luser32 \
    $(pkg-config --libs tesseract)
