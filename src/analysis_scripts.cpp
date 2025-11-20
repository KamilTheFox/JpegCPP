#include "analysis_scripts.h"
#include "jpeg_decoder.h"
#include <iostream>
#include <iomanip>
#include <algorithm> 

using namespace std;

void AnalysisScripts::testFullCycle(int width, int height, int quality) {
    cout << "\n=== Full JPEG Cycle Test " << width << "x" << height << " Q" << quality << " ===" << endl;
    
    // 1. Создаем тестовое изображение
    auto original = RgbImage::createTestImage(width, height);
    cout << "Created test image: " << width << "x" << height << endl;
    
    // 2. Кодируем в JPEG
    auto encoder = createJpegEncoder(quality);
    auto encodedData = encoder->encode(original);
    cout << "JPEG encoded: " << encodedData.compressedData.size() << " bytes" << endl;
    
    // 3. Декодируем обратно
    auto decoder = createJpegDecoder(encodedData.quantizationTable);
    auto reconstructed = decoder->decode(encodedData);
    cout << "JPEG decoded: " << reconstructed.getWidth() << "x" << reconstructed.getHeight() << endl;
    
    // 4. Сравниваем
    ImageMetrics::printComparisonReport(original, reconstructed, 
                                      encodedData.compressedData.size(),
                                      "Full Cycle Test");
}

void AnalysisScripts::qualitySweepTest(int width, int height) {
    cout << "\n=== Quality Sweep Test " << width << "x" << height << " ===" << endl;
    cout << "Quality\tSize(bytes)\tRatio\tPSNR(dB)\tSSIM" << endl;
    cout << "-------\t-----------\t-----\t--------\t----" << endl;
    
    auto original = RgbImage::createTestImage(width, height);
    
    vector<int> qualities = {10, 25, 50, 75, 90, 95, 100};
    
    for (int quality : qualities) {
        auto encoder = createJpegEncoder(quality);
        auto encodedData = encoder->encode(original);
        
        auto decoder = createJpegDecoder(encodedData.quantizationTable);
        auto reconstructed = decoder->decode(encodedData);
        
        double psnr = ImageMetrics::peakSignalToNoiseRatio(original, reconstructed);
        double ssim = ImageMetrics::structuralSimilarityIndex(original, reconstructed);
        double ratio = ImageMetrics::compressionRatio(original, encodedData.compressedData.size());
        
        cout << quality << "\t" 
             << encodedData.compressedData.size() << "\t\t"
             << fixed << setprecision(2) << ratio << "\t"
             << setprecision(2) << psnr << "\t\t"
             << setprecision(3) << ssim << endl;
    }
}

void AnalysisScripts::compareDifferentSizes() {
    cout << "\n=== Size Comparison Test ===" << endl;
    
    vector<pair<int, int>> sizes = {{64, 64}, {128, 128}, {256, 256}};
    vector<int> qualities = {25, 50, 75};
    
    for (auto [width, height] : sizes) {
        for (int quality : qualities) {
            testFullCycle(width, height, quality);
        }
    }
}

void AnalysisScripts::generateQualityReport() {
    cout << "\n" << string(60, '=') << endl;
    cout << "JPEG COMPRESSION QUALITY ANALYSIS REPORT" << endl;
    cout << string(60, '=') << endl;
    
    // Тест разных уровней качества
    qualitySweepTest(128, 128);
    
    // Тест разных размеров
    compareDifferentSizes();
    
    cout << "\n" << string(60, '=') << endl;
    cout << "REPORT COMPLETED" << endl;
    cout << string(60, '=') << endl;
}
