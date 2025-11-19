#include <iostream>
#include <chrono>
#include <vector>
#include <utility>
#include <thread>

#include "sequential_processors.h"
#include "multy_thread.h"

using namespace std;
using namespace std::chrono;

void printCompressionInfo(const RgbImage& image, 
                          const JpegEncodedData& encodedData, 
                          long long encodingTimeMs) {
    cout << "=== JPEG Compression Results ===" << endl;
    cout << "Original image: " << image.getWidth() << "x" << image.getHeight() << endl;
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    cout << "Original size: ~" << originalSize << " bytes (RGB)" << endl;
    cout << "Compressed size: " << encodedData.compressedData.size() << " bytes" << endl;
    
    double compressionRatio = originalSize / static_cast<double>(encodedData.compressedData.size());
    cout << "Compression ratio: " << compressionRatio << ":1" << endl;
    
    double compressionPercentage = 
        (1.0 - (encodedData.compressedData.size() / static_cast<double>(originalSize))) * 100.0;
    cout << "Space savings: " << compressionPercentage << "%" << endl;
    
    cout << "Encoding time: " << encodingTimeMs << " ms" << endl;
    cout << "Huffman DC table size: " << encodedData.dcLuminanceTable.size() << " symbols" << endl;
    cout << "Huffman AC table size: " << encodedData.acLuminanceTable.size() << " symbols" << endl;
}

int main() {
    cout << "JPEG Compressor C++ - Sequential vs Multithread" << endl;
    
    try {
        // Тестируем разные размеры изображений
        vector<pair<int, int>> testSizes = {{64, 64}, {128, 128}, {256, 256}};
        
        for (const auto& [width, height] : testSizes) {
            cout << "\n======================================================" << endl;
            cout << "--- Testing " << width << "x" << height << " image ---" << endl;
            
            // Создаем тестовое изображение
            auto testImage = RgbImage::createTestImage(width, height);

            long long seqTimeMs  = 0;
            long long mtTimeMs   = 0;

            // === 1) ОДНОПОТОЧНАЯ ВЕРСИЯ ===
            {
                auto colorConverter = make_unique<SequentialColorConverter>();
                auto dctTransform   = make_unique<SequentialDctTransform>();
                auto quantizer      = make_unique<SequentialQuantizer>(75); // quality = 75
                auto blockProcessor = make_unique<SequentialBlockProcessor>(
                    move(dctTransform), move(quantizer));
                auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
                
                auto startTime = high_resolution_clock::now();
                
                JpegEncoder encoder(move(colorConverter), 
                                    move(blockProcessor), 
                                    move(huffmanEncoder));
                auto encodedData = encoder.encode(testImage);
                
                auto endTime      = high_resolution_clock::now();
                auto encodingTime = duration_cast<milliseconds>(endTime - startTime);
                seqTimeMs = encodingTime.count();
                
                cout << "\n[Sequential]" << endl;
                printCompressionInfo(testImage, encodedData, seqTimeMs);
                
                if (encodedData.compressedData.empty()) {
                    cerr << "Warning: Compressed data is empty!" << endl;
                }
                
                size_t originalSize = static_cast<size_t>(width) * height * 3;
                if (encodedData.compressedData.size() > originalSize) {
                    cout << "Note: Compression actually increased size (normal for small images)" << endl;
                }
            }

            // === 2) МНОГОПОТОЧНАЯ ВЕРСИЯ ===
            {
                int threads = static_cast<int>(std::thread::hardware_concurrency());
                if (threads == 0) threads = 4; // дефолт, если ОС не вернула значение
                
                auto colorConverter = make_unique<MultiThreadColorConverter>(threads);
                auto dctTransform   = make_unique<SequentialDctTransform>();
                auto quantizer      = make_unique<SequentialQuantizer>(75);
                auto blockProcessor = make_unique<MultiThreadBlockProcessor>(
                    move(dctTransform), move(quantizer), threads);
                auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
                
                auto startTime = high_resolution_clock::now();
                
                JpegEncoder encoder(move(colorConverter), 
                                    move(blockProcessor), 
                                    move(huffmanEncoder));
                auto encodedData = encoder.encode(testImage);
                
                auto endTime      = high_resolution_clock::now();
                auto encodingTime = duration_cast<milliseconds>(endTime - startTime);
                mtTimeMs = encodingTime.count();
                
                cout << "\n[Multithread] (" << threads << " threads)" << endl;
                printCompressionInfo(testImage, encodedData, mtTimeMs);

                if (mtTimeMs > 0 && seqTimeMs > 0) {
                    double speedup = static_cast<double>(seqTimeMs) / mtTimeMs;
                    cout << "Speedup (Sequential / Multithread): " << speedup << "x" << endl;
                }
            }
        }
        
        // Тест разных уровней качества (оставим последовательную реализацию для наглядности)
        cout << "\n--- Testing Different Quality Levels (Sequential) ---" << endl;
        auto testImage = RgbImage::createTestImage(128, 128);
        
        vector<int> qualityLevels = {25, 50, 75, 95};
        for (int quality : qualityLevels) {
            auto colorConverter = make_unique<SequentialColorConverter>();
            auto dctTransform   = make_unique<SequentialDctTransform>();
            auto quantizer      = make_unique<SequentialQuantizer>(quality);
            auto blockProcessor = make_unique<SequentialBlockProcessor>(
                move(dctTransform), move(quantizer));
            auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
            
            JpegEncoder encoder(move(colorConverter), 
                                move(blockProcessor), 
                                move(huffmanEncoder));
            auto encodedData = encoder.encode(testImage);
            
            size_t originalSize = static_cast<size_t>(128) * 128 * 3;
            double ratio = originalSize / static_cast<double>(encodedData.compressedData.size());
            
            cout << "Quality " << quality << ": " 
                 << encodedData.compressedData.size() 
                 << " bytes, ratio: " << ratio << ":1" << endl;
        }
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    cout << "\nAll tests completed successfully!" << endl;
    return 0;
}
