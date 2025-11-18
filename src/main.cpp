#include <iostream>
#include <chrono>
#include "sequential_processors.h"

using namespace std;
using namespace std::chrono;

void printCompressionInfo(const RgbImage& image, const JpegEncodedData& encodedData, 
                         long long encodingTimeMs) {
    cout << "=== JPEG Compression Results ===" << endl;
    cout << "Original image: " << image.getWidth() << "x" << image.getHeight() << endl;
    
    // Используем static_cast для избежания предупреждений
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    cout << "Original size: ~" << originalSize << " bytes (RGB)" << endl;
    cout << "Compressed size: " << encodedData.compressedData.size() << " bytes" << endl;
    
    double compressionRatio = originalSize / static_cast<double>(encodedData.compressedData.size());
    cout << "Compression ratio: " << compressionRatio << ":1" << endl;
    
    double compressionPercentage = (1.0 - (encodedData.compressedData.size() / static_cast<double>(originalSize))) * 100;
    cout << "Space savings: " << compressionPercentage << "%" << endl;
    
    cout << "Encoding time: " << encodingTimeMs << " ms" << endl;
    cout << "Huffman DC table size: " << encodedData.dcLuminanceTable.size() << " symbols" << endl;
    cout << "Huffman AC table size: " << encodedData.acLuminanceTable.size() << " symbols" << endl;
}

int main() {
    cout << "JPEG Compressor C++ - Sequential Implementation" << endl;
    
    try {
        // Тестируем разные размеры изображений
        vector<pair<int, int>> testSizes = {{64, 64}, {128, 128}, {256, 256}};
        
        for (const auto& [width, height] : testSizes) {
            cout << "\n--- Testing " << width << "x" << height << " image ---" << endl;
            
            // Создаем тестовое изображение
            auto testImage = RgbImage::createTestImage(width, height);
            
            // Создаем процессоры
            auto colorConverter = make_unique<SequentialColorConverter>();
            auto dctTransform = make_unique<SequentialDctTransform>();
            auto quantizer = make_unique<SequentialQuantizer>(75); // quality = 75
            auto blockProcessor = make_unique<SequentialBlockProcessor>(
                move(dctTransform), move(quantizer));
            auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
            
            // Измеряем время кодирования
            auto startTime = high_resolution_clock::now();
            
            // Создаем энкодер и кодируем
            JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
            auto encodedData = encoder.encode(testImage);
            
            auto endTime = high_resolution_clock::now();
            auto encodingTime = duration_cast<milliseconds>(endTime - startTime);
            
            // Выводим информацию о сжатии
            printCompressionInfo(testImage, encodedData, encodingTime.count());
            
            // Простая проверка целостности данных
            if (encodedData.compressedData.empty()) {
                cerr << "Warning: Compressed data is empty!" << endl;
            }
            
            // Исправляем сравнение signed/unsigned
            size_t originalSize = static_cast<size_t>(width) * height * 3;
            if (encodedData.compressedData.size() > originalSize) {
                cout << "Note: Compression actually increased size (normal for small images)" << endl;
            }
        }
        
        // Тестируем разные уровни качества
        cout << "\n--- Testing Different Quality Levels ---" << endl;
        auto testImage = RgbImage::createTestImage(128, 128);
        
        vector<int> qualityLevels = {25, 50, 75, 95};
        for (int quality : qualityLevels) {
            auto colorConverter = make_unique<SequentialColorConverter>();
            auto dctTransform = make_unique<SequentialDctTransform>();
            auto quantizer = make_unique<SequentialQuantizer>(quality);
            auto blockProcessor = make_unique<SequentialBlockProcessor>(
                move(dctTransform), move(quantizer));
            auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
            
            JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
            auto encodedData = encoder.encode(testImage);
            
            size_t originalSize = static_cast<size_t>(128) * 128 * 3;
            double ratio = originalSize / static_cast<double>(encodedData.compressedData.size());
            
            cout << "Quality " << quality << ": " << encodedData.compressedData.size() 
                 << " bytes, ratio: " << ratio << ":1" << endl;
        }
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    cout << "\nAll tests completed successfully!" << endl;
    return 0;
}