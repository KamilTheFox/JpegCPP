#include "image_types.h"
#include <stdexcept>
#include <algorithm>

using namespace std;

// Реализация RgbImage
RgbImage::RgbImage(int width, int height) : width(width), height(height) {
    if (width <= 0 || height <= 0) {
        throw invalid_argument("Dimensions must be positive");
    }
    data.resize(width * height * 3, 0);
}

RgbImage::RgbImage(int width, int height, const vector<unsigned char>& rgbData) 
    : width(width), height(height), data(rgbData) {
    if (rgbData.size() != width * height * 3) {
        throw invalid_argument("Data size mismatch");
    }
}

tuple<unsigned char, unsigned char, unsigned char> RgbImage::getPixel(int x, int y) const {
    int index = (y * width + x) * 3;
    return make_tuple(data[index], data[index + 1], data[index + 2]);
}

void RgbImage::setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
    int index = (y * width + x) * 3;
    data[index] = r;
    data[index + 1] = g;
    data[index + 2] = b;
}

RgbImage RgbImage::createTestImage(int width, int height) {
    RgbImage image(width, height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Градиент или шахматная доска
            unsigned char r = static_cast<unsigned char>(x * 255 / width);
            unsigned char g = static_cast<unsigned char>(y * 255 / height);
            unsigned char b = static_cast<unsigned char>((x + y) % 255);
            image.setPixel(x, y, r, g, b);
        }
    }
    return image;
}

// Реализация YCbCrImage
YCbCrImage::YCbCrImage(int width, int height) : width(width), height(height) {
    if (width <= 0 || height <= 0) {
        throw invalid_argument("Dimensions must be positive");
    }
    Y.resize(height, vector<unsigned char>(width, 0));
    Cb.resize(height, vector<unsigned char>(width, 0));
    Cr.resize(height, vector<unsigned char>(width, 0));
}

tuple<unsigned char, unsigned char, unsigned char> YCbCrImage::getPixel(int x, int y) const {
    return make_tuple(Y[y][x], Cb[y][x], Cr[y][x]);
}

void YCbCrImage::setPixel(int x, int y, unsigned char yVal, unsigned char cbVal, unsigned char crVal) {
    Y[y][x] = yVal;
    Cb[y][x] = cbVal;
    Cr[y][x] = crVal;
}