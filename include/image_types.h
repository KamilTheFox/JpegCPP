#ifndef IMAGE_TYPES_H
#define IMAGE_TYPES_H

#include <vector>
#include <memory>
#include <unordered_map>

class RgbImage {
private:
    std::vector<unsigned char> data;
    int width;
    int height;

public:
    RgbImage(int width, int height);
    RgbImage(int width, int height, const std::vector<unsigned char>& rgbData);
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getPixelCount() const { return width * height; }
    
    std::tuple<unsigned char, unsigned char, unsigned char> getPixel(int x, int y) const;
    void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
    
    const std::vector<unsigned char>& getRawData() const { return data; }
    
    static RgbImage createTestImage(int width, int height);
    // static RgbImage loadFromFile(const std::string& path); // Пока без реализации файлового ввода
};

class YCbCrImage {
private:
    std::vector<std::vector<unsigned char>> Y;
    std::vector<std::vector<unsigned char>> Cb;
    std::vector<std::vector<unsigned char>> Cr;
    int width;
    int height;

public:
    YCbCrImage(int width, int height);
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    std::tuple<unsigned char, unsigned char, unsigned char> getPixel(int x, int y) const;
    void setPixel(int x, int y, unsigned char yVal, unsigned char cbVal, unsigned char crVal);
    
    const std::vector<std::vector<unsigned char>>& getY() const { return Y; }
    const std::vector<std::vector<unsigned char>>& getCb() const { return Cb; }
    const std::vector<std::vector<unsigned char>>& getCr() const { return Cr; }
};

struct JpegEncodedData {
    std::vector<unsigned char> compressedData;
    std::unordered_map<int, std::pair<int, int>> dcLuminanceTable;
    std::unordered_map<int, std::pair<int, int>> acLuminanceTable;
    std::vector<std::vector<int>> quantizationTable;
    int width;
    int height;
};

#endif