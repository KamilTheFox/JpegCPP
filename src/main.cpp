#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include "sequential_processors.h"
#include "OpenMPBlockProcessor.h"
#include "OpenMPDctTransform.h"
#include "OpenMPQuantizer.h"
#include "pipeline_processor.h"

using namespace std;
using namespace std::chrono;

struct BenchmarkResult {
    string name;
    long long timeMs;
    size_t compressedSize;
    double compressionRatio;
};

void printResults(const vector<BenchmarkResult>& results) {
    cout << "\n=== Performance Comparison ===" << endl;
    cout << left << setw(45) << "Method" 
         << right << setw(12) << "Time (ms)" 
         << setw(15) << "Size (bytes)" 
         << setw(12) << "Ratio" 
         << setw(12) << "Speedup" << endl;
    cout << string(96, '-') << endl;
    
    long long baselineTime = results[0].timeMs;
    
    for (const auto& r : results) {
        double speedup = static_cast<double>(baselineTime) / r.timeMs;
        cout << left << setw(45) << r.name
             << right << setw(12) << r.timeMs
             << setw(15) << r.compressedSize
             << setw(12) << fixed << setprecision(2) << r.compressionRatio
             << setw(12) << fixed << setprecision(2) << speedup << "x" << endl;
    }
}

BenchmarkResult benchmarkSequential(const RgbImage& image, const string& name) {
    auto colorConverter = make_unique<SequentialColorConverter>();
    auto dctTransform = make_unique<SequentialDctTransform>();
    auto quantizer = make_unique<SequentialQuantizer>(75);
    auto blockProcessor = make_unique<SequentialBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
    
    auto start = high_resolution_clock::now();
    auto result = encoder.encode(image);
    auto end = high_resolution_clock::now();
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    
    return BenchmarkResult{
        name,
        duration_cast<milliseconds>(end - start).count(),
        result.compressedData.size(),
        static_cast<double>(originalSize) / result.compressedData.size()
    };
}

BenchmarkResult benchmarkOpenMP(const RgbImage& image, const string& name) {
    auto colorConverter = make_unique<SequentialColorConverter>();
    auto dctTransform = make_unique<OpenMPDctTransform>();
    auto quantizer = make_unique<OpenMPQuantizer>(75);
    auto blockProcessor = make_unique<OpenMPBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
    
    auto start = high_resolution_clock::now();
    auto result = encoder.encode(image);
    auto end = high_resolution_clock::now();
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    
    return BenchmarkResult{
        name,
        duration_cast<milliseconds>(end - start).count(),
        result.compressedData.size(),
        static_cast<double>(originalSize) / result.compressedData.size()
    };
}

BenchmarkResult benchmarkPipeline(const RgbImage& image, const string& name, int threadCount) {
    auto colorConverter = make_unique<PipelineColorConverter>();
    auto dctTransform = make_unique<PipelineDctTransform>();
    auto quantizer = make_unique<PipelineQuantizer>(75);
    auto huffmanEncoder = make_unique<PipelineHuffmanEncoder>();
    
    PipelineJpegEncoder encoder(
        move(colorConverter),
        move(dctTransform),
        move(quantizer),
        move(huffmanEncoder),
        threadCount
    );
    
    auto start = high_resolution_clock::now();
    auto result = encoder.encode(image);
    auto end = high_resolution_clock::now();
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    
    return BenchmarkResult{
        name,
        duration_cast<milliseconds>(end - start).count(),
        result.compressedData.size(),
        static_cast<double>(originalSize) / result.compressedData.size()
    };
}

BenchmarkResult benchmarkMix1(const RgbImage& image, const string& name) {
    // Mix 1: Pipeline ColorConverter + OpenMP BlockProcessor
    auto colorConverter = make_unique<PipelineColorConverter>();
    auto dctTransform = make_unique<OpenMPDctTransform>();
    auto quantizer = make_unique<OpenMPQuantizer>(75);
    auto blockProcessor = make_unique<OpenMPBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
    
    auto start = high_resolution_clock::now();
    auto result = encoder.encode(image);
    auto end = high_resolution_clock::now();
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    
    return BenchmarkResult{
        name,
        duration_cast<milliseconds>(end - start).count(),
        result.compressedData.size(),
        static_cast<double>(originalSize) / result.compressedData.size()
    };
}

BenchmarkResult benchmarkMix2(const RgbImage& image, const string& name) {
    // Mix 2: Sequential ColorConverter + Pipeline BlockProcessor
    auto colorConverter = make_unique<SequentialColorConverter>();
    auto dctTransform = make_unique<PipelineDctTransform>();
    auto quantizer = make_unique<PipelineQuantizer>(75);
    auto blockProcessor = make_unique<PipelineBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    JpegEncoder encoder(move(colorConverter), move(blockProcessor), move(huffmanEncoder));
    
    auto start = high_resolution_clock::now();
    auto result = encoder.encode(image);
    auto end = high_resolution_clock::now();
    
    size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
    
    return BenchmarkResult{
        name,
        duration_cast<milliseconds>(end - start).count(),
        result.compressedData.size(),
        static_cast<double>(originalSize) / result.compressedData.size()
    };
}

int main() {
    cout << "JPEG Compressor - Parallelization Methods Comparison" << endl;
    cout << "Hardware threads available: " << thread::hardware_concurrency() << endl;
    
    vector<pair<int, int>> testSizes = {
        {256, 256},
        {512, 512},
        {1024, 1024}
    };
    
    for (const auto& [width, height] : testSizes) {
        cout << "\n" << string(96, '=') << endl;
        cout << "Testing " << width << "x" << height << " image" << endl;
        cout << string(96, '=') << endl;
        
        auto testImage = RgbImage::createTestImage(width, height);
        vector<BenchmarkResult> results;
        
        try {
            // 1. Sequential baseline
            cout << "Running Sequential (baseline)..." << flush;
            results.push_back(benchmarkSequential(testImage, "Sequential (baseline)"));
            cout << " Done (" << results.back().timeMs << " ms)" << endl;
            
            // 2. OpenMP
            cout << "Running OpenMP..." << flush;
            results.push_back(benchmarkOpenMP(testImage, "OpenMP (DCT + Quantizer)"));
            cout << " Done (" << results.back().timeMs << " ms)" << endl;
            
            // 3. Pipeline with different thread counts
            vector<int> threadCounts = {2, 4, 8};
            for (int tc : threadCounts) {
                if (tc <= thread::hardware_concurrency()) {
                    cout << "Running Pipeline (" << tc << " threads)..." << flush;
                    results.push_back(benchmarkPipeline(testImage, 
                        "Pipeline (" + to_string(tc) + " threads)", tc));
                    cout << " Done (" << results.back().timeMs << " ms)" << endl;
                }
            }
            
            // 4. Mix 1: Pipeline ColorConv + OpenMP
            cout << "Running Mix 1 (Pipeline ColorConv + OpenMP)..." << flush;
            results.push_back(benchmarkMix1(testImage, 
                "Mix: Pipeline ColorConv + OpenMP"));
            cout << " Done (" << results.back().timeMs << " ms)" << endl;
            
            // 5. Mix 2: Sequential ColorConv + Pipeline BlockProc
            cout << "Running Mix 2 (Sequential + Pipeline BlockProc)..." << flush;
            results.push_back(benchmarkMix2(testImage, 
                "Mix: Sequential + Pipeline BlockProc"));
            cout << " Done (" << results.back().timeMs << " ms)" << endl;
            
            printResults(results);
            
        } catch (const exception& e) {
            cerr << "\nError during benchmark: " << e.what() << endl;
        }
    }
    
    cout << "\n" << string(96, '=') << endl;
    cout << "Benchmarks completed!" << endl;
    cout << string(96, '=') << endl;
    
    return 0;
}