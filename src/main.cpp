#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include "sequential_processors.h"
#include "OpenMPBlockProcessor.h"
#include "OpenMPDctTransform.h"
#include "OpenMPQuantizer.h"
#include "pipeline_processor.h"
#include "multy_thread.h"

using namespace std;
using namespace std::chrono;

struct BenchmarkResult {
    string name;
    long long totalTimeMs;
    long long avgTimeMs;
    size_t avgCompressedSize;
    double avgCompressionRatio;
};

void printResults(const vector<BenchmarkResult>& results) {
    cout << "\n=== Performance Comparison ===" << endl;
    cout << left << setw(50) << "Method" 
         << right << setw(12) << "Avg (ms)" 
         << setw(12) << "Total (ms)"
         << setw(15) << "Size (bytes)" 
         << setw(12) << "Ratio" 
         << setw(12) << "Speedup" << endl;
    cout << string(113, '-') << endl;
    
    long long baselineTime = results[0].avgTimeMs;
    
    for (const auto& r : results) {
        double speedup = static_cast<double>(baselineTime) / r.avgTimeMs;
        cout << left << setw(50) << r.name
             << right << setw(12) << r.avgTimeMs
             << setw(12) << r.totalTimeMs
             << setw(15) << r.avgCompressedSize
             << setw(12) << fixed << setprecision(2) << r.avgCompressionRatio
             << setw(12) << fixed << setprecision(2) << speedup << "x" << endl;
    }
}

template<typename BenchFunc>
BenchmarkResult runBenchmark(const string& name, const vector<RgbImage>& images, BenchFunc func) {
    cout << "Running " << name << "..." << flush;
    
    vector<long long> times;
    vector<size_t> sizes;
    vector<double> ratios;
    
    auto totalStart = high_resolution_clock::now();
    
    for (const auto& image : images) {
        auto start = high_resolution_clock::now();
        auto result = func(image);
        auto end = high_resolution_clock::now();
        
        long long time = duration_cast<milliseconds>(end - start).count();
        size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
        
        times.push_back(time);
        sizes.push_back(result.compressedData.size());
        ratios.push_back(static_cast<double>(originalSize) / result.compressedData.size());
    }
    
    auto totalEnd = high_resolution_clock::now();
    long long totalTime = duration_cast<milliseconds>(totalEnd - totalStart).count();
    
    long long avgTime = totalTime / images.size();
    size_t avgSize = 0;
    double avgRatio = 0;
    
    for (size_t i = 0; i < sizes.size(); i++) {
        avgSize += sizes[i];
        avgRatio += ratios[i];
    }
    avgSize /= sizes.size();
    avgRatio /= ratios.size();
    
    cout << " Done (" << totalTime << " ms total, " << avgTime << " ms avg)" << endl;
    
    return BenchmarkResult{name, totalTime, avgTime, avgSize, avgRatio};
}

int main() {
    cout << "JPEG Compressor - Aggressive Parallelization Benchmark" << endl;
    cout << "Hardware threads available: " << thread::hardware_concurrency() << endl;
    cout << "Images per test: 10" << endl;
    
    vector<pair<int, int>> testSizes = {
        {512, 512},
        {1024, 1024},
        {2048, 2048}
    };
    
    for (const auto& [width, height] : testSizes) {
        cout << "\n" << string(113, '=') << endl;
        cout << "Testing " << width << "x" << height << " image (10 iterations)" << endl;
        cout << string(113, '=') << endl;
        
        // Создаем 10 разных изображений
        vector<RgbImage> images;
        for (int i = 0; i < 10; i++) {
            images.push_back(RgbImage::createTestImage(width, height));
        }
        
        vector<BenchmarkResult> results;
        
        try {
            // 1. Sequential baseline
            results.push_back(runBenchmark(
                "1. Sequential (baseline)",
                images,
                [](const RgbImage& img) {
                    auto colorConv = make_unique<SequentialColorConverter>();
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(75);
                    auto blockProc = make_unique<SequentialBlockProcessor>(move(dct), move(quant));
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            // 2. OpenMP
            results.push_back(runBenchmark(
                "2. OpenMP (DCT + Quantizer)",
                images,
                [](const RgbImage& img) {
                    auto colorConv = make_unique<SequentialColorConverter>();
                    auto dct = make_unique<OpenMPDctTransform>();
                    auto quant = make_unique<OpenMPQuantizer>(75);
                    auto blockProc = make_unique<OpenMPBlockProcessor>(move(dct), move(quant));
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            // 3. MultiThread
            int threads = thread::hardware_concurrency();
            results.push_back(runBenchmark(
                "3. MultiThread (" + to_string(threads) + " threads)",
                images,
                [threads](const RgbImage& img) {
                    auto colorConv = make_unique<MultiThreadColorConverter>(threads);
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(75);
                    auto blockProc = make_unique<MultiThreadBlockProcessor>(move(dct), move(quant), threads);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            // 4. Pipeline
            results.push_back(runBenchmark(
                "4. Pipeline (" + to_string(threads) + " threads)",
                images,
                [threads](const RgbImage& img) {
                    auto colorConv = make_unique<SequentialColorConverter>();
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(75);
                    auto blockProc = make_unique<PipelineBlockProcessor>(move(dct), move(quant), threads);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            // 5. Mix: MultiThread ColorConv + OpenMP BlockProc
            results.push_back(runBenchmark(
                "5. Mix: MultiThread ColorConv + OpenMP",
                images,
                [threads](const RgbImage& img) {
                    auto colorConv = make_unique<MultiThreadColorConverter>(threads);
                    auto dct = make_unique<OpenMPDctTransform>();
                    auto quant = make_unique<OpenMPQuantizer>(75);
                    auto blockProc = make_unique<OpenMPBlockProcessor>(move(dct), move(quant));
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            // 6. Mix: MultiThread ColorConv + Pipeline BlockProc
            results.push_back(runBenchmark(
                "6. Mix: MultiThread ColorConv + Pipeline",
                images,
                [threads](const RgbImage& img) {
                    auto colorConv = make_unique<MultiThreadColorConverter>(threads);
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(75);
                    auto blockProc = make_unique<PipelineBlockProcessor>(move(dct), move(quant), threads);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    return encoder.encode(img);
                }
            ));
            
            printResults(results);
            
        } catch (const exception& e) {
            cerr << "\nError during benchmark: " << e.what() << endl;
        }
    }
    
    cout << "\n" << string(113, '=') << endl;
    cout << "Benchmarks completed!" << endl;
    cout << string(113, '=') << endl;
    
    return 0;
}