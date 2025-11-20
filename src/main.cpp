#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <functional>
#include <omp.h>
#include "sequential_processors.h"
#include "OpenMPBlockProcessor.h"
#include "OpenMPDctTransform.h"
#include "OpenMPQuantizer.h"
#include "pipeline_processor.h"
#include "multy_thread.h"
#include "jpeg_decoder.h"
#include "image_metrics.h"

using namespace std;
using namespace std::chrono;

struct BenchmarkResult {
    string name;
    long long totalTimeMs;
    long long avgTimeMs;
    size_t avgCompressedSize;
    double avgCompressionRatio;
    double avgPsnr;
    double avgSsim;
};

struct EncodingResult {
    JpegEncodedData encoded;
    vector<QuantizedBlock> blocks;
};

void printResults(const vector<BenchmarkResult>& results) {
    cout << "\n=== Performance Comparison ===" << endl;
    cout << left << setw(50) << "Method" 
         << right << setw(10) << "Avg(ms)" 
         << setw(12) << "Total(ms)"
         << setw(12) << "Size" 
         << setw(10) << "Ratio"
         << setw(10) << "PSNR"
         << setw(10) << "SSIM"
         << setw(10) << "Speedup" << endl;
    cout << string(124, '-') << endl;
    
    long long baselineTime = results[0].avgTimeMs;
    
    for (const auto& r : results) {
        double speedup = (r.avgTimeMs > 0) ? static_cast<double>(baselineTime) / r.avgTimeMs : 0.0;
        cout << left << setw(50) << r.name
             << right << setw(10) << r.avgTimeMs
             << setw(12) << r.totalTimeMs
             << setw(12) << r.avgCompressedSize
             << setw(10) << fixed << setprecision(2) << r.avgCompressionRatio
             << setw(10) << fixed << setprecision(2) << r.avgPsnr
             << setw(10) << fixed << setprecision(4) << r.avgSsim
             << setw(10) << fixed << setprecision(2) << speedup << "x" << endl;
    }
}

using EncoderFactory = function<EncodingResult(const RgbImage&)>;

// Вспомогательный класс для захвата блоков
class BlockCapturingProcessor : public IBlockProcessor {
private:
    unique_ptr<IBlockProcessor> inner;
    vector<QuantizedBlock>* capturedBlocks;

public:
    BlockCapturingProcessor(unique_ptr<IBlockProcessor> processor, vector<QuantizedBlock>* blocks)
        : inner(move(processor)), capturedBlocks(blocks) {}
    
    vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override {
        auto blocks = inner->processBlocks(image);
        *capturedBlocks = blocks;
        return blocks;
    }
};

BenchmarkResult runBenchmark(const string& name, 
                             const vector<RgbImage>& images,
                             EncoderFactory factory,
                             const vector<vector<int>>& quantTable,
                             int iterations = 10) {
    cout << "Running " << name << "..." << flush;
    
    vector<long long> times;
    vector<size_t> sizes;
    vector<double> ratios;
    
    vector<EncodingResult> encodingResults;
    encodingResults.reserve(iterations);
    
    // ========== ЗАМЕР ТОЛЬКО КОДИРОВАНИЯ ==========
    auto totalStart = high_resolution_clock::now();
    
    for (int iter = 0; iter < iterations; iter++) {
        // Используем одно и то же изображение для всех итераций
        const auto& image = images[0];  // первое изображение
        
        auto start = high_resolution_clock::now();
        auto result = factory(image);
        auto end = high_resolution_clock::now();
        
        long long time = duration_cast<milliseconds>(end - start).count();
        size_t originalSize = static_cast<size_t>(image.getWidth()) * image.getHeight() * 3;
        
        times.push_back(time);
        sizes.push_back(result.encoded.compressedData.size());
        ratios.push_back(static_cast<double>(originalSize) / result.encoded.compressedData.size());
        
        encodingResults.push_back(move(result));
    }
    
    auto totalEnd = high_resolution_clock::now();
    long long totalTime = duration_cast<milliseconds>(totalEnd - totalStart).count();
    
    // ==========  УСРЕДНЕНИЕ ==========
    long long avgTime = 0;
    size_t avgSize = 0;
    double avgRatio = 0;
    
    for (int i = 0; i < iterations; i++) {
        avgTime += times[i];
        avgSize += sizes[i];
        avgRatio += ratios[i];
    }
    avgTime /= iterations;  // Усредняем отдельные замеры
    avgSize /= iterations;
    avgRatio /= iterations;
    
    cout << " " << totalTime << " ms";
    
    // ========== ПРОВЕРКА КАЧЕСТВА ОТДЕЛЬНО (не входит в замер) ==========
    vector<double> psnrs;
    vector<double> ssims;
    
    auto decoder = createJpegDecoder(quantTable);
    
    for (int iter = 0; iter < iterations; iter++) {
        const auto& image = images[iter % images.size()];
        const auto& result = encodingResults[iter];
        
        auto reconstructed = decoder->decodeFromBlocks(result.blocks, 
                                                       image.getWidth(), 
                                                       image.getHeight());
        
        psnrs.push_back(ImageMetrics::peakSignalToNoiseRatio(image, reconstructed));
        ssims.push_back(ImageMetrics::structuralSimilarityIndex(image, reconstructed));
    }
    
    double avgPsnr = 0, avgSsim = 0;
    for (int i = 0; i < iterations; i++) {
        avgPsnr += psnrs[i];
        avgSsim += ssims[i];
    }
    avgPsnr /= iterations;
    avgSsim /= iterations;
    
    cout << " (PSNR: " << fixed << setprecision(1) << avgPsnr << " dB)" << endl;
    
    return BenchmarkResult{name, totalTime, avgTime, avgSize, avgRatio, avgPsnr, avgSsim};
}

int main() {
    int maxThreads = thread::hardware_concurrency();
    
    cout << "JPEG Compressor - Parallelization Benchmark (Encoding Only)" << endl;
    cout << "Hardware threads available: " << maxThreads << endl;
    cout << "Iterations per test: 10" << endl;
    cout << "NOTE: Only encoding time is measured, decoding/metrics calculated separately" << endl;
    
    int quality = 75;
    auto quantTable = SequentialQuantizer::defaultQuantizationTable();
    
    vector<pair<int, int>> testSizes = {
        {1024, 1024},
        {2048, 2048}
    };
    
    for (const auto& [width, height] : testSizes) {
        cout << "\n" << string(124, '=') << endl;
        cout << "Testing " << width << "x" << height << " image" << endl;
        cout << string(124, '=') << endl;
        
        vector<RgbImage> images;
        for (int i = 0; i < 3; i++) {
            images.push_back(RgbImage::createTestImage(width, height));
        }
        
        vector<BenchmarkResult> results;
        
        try {
            // 1. Sequential baseline
            results.push_back(runBenchmark(
                "1. Sequential (baseline)",
                images,
                [quality](const RgbImage& img) -> EncodingResult {
                    vector<QuantizedBlock> blocks;
                    auto colorConv = make_unique<SequentialColorConverter>();
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(quality);
                    auto innerProc = make_unique<SequentialBlockProcessor>(move(dct), move(quant));
                    auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    auto encoded = encoder.encode(img);
                    return {encoded, blocks};
                },
                quantTable
            ));
            
            // 2. OpenMP с разным числом потоков
            for (int numThreads : {2, 4}) {
                results.push_back(runBenchmark(
                    "2. OpenMP (" + to_string(numThreads) + " threads)",
                    images,
                    [quality, numThreads](const RgbImage& img) -> EncodingResult {
                        omp_set_num_threads(numThreads);
                        vector<QuantizedBlock> blocks;
                        auto colorConv = make_unique<SequentialColorConverter>();
                        auto dct = make_unique<OpenMPDctTransform>();
                        auto quant = make_unique<OpenMPQuantizer>(quality);
                        auto innerProc = make_unique<OpenMPBlockProcessor>(move(dct), move(quant));
                        auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                        auto huffman = make_unique<SequentialHuffmanEncoder>();
                        JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                        auto encoded = encoder.encode(img);
                        return {encoded, blocks};
                    },
                    quantTable
                ));
            }
            
            // 3. MultiThread с разным числом потоков
            for (int numThreads : {2, 4, 6}) {
                results.push_back(runBenchmark(
                    "3. MultiThread (" + to_string(numThreads) + " threads)",
                    images,
                    [quality, numThreads](const RgbImage& img) -> EncodingResult {
                        vector<QuantizedBlock> blocks;
                        auto colorConv = make_unique<MultiThreadColorConverter>(numThreads);
                        auto dct = make_unique<SequentialDctTransform>();
                        auto quant = make_unique<SequentialQuantizer>(quality);
                        auto innerProc = make_unique<MultiThreadBlockProcessor>(move(dct), move(quant), numThreads);
                        auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                        auto huffman = make_unique<SequentialHuffmanEncoder>();
                        JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                        auto encoded = encoder.encode(img);
                        return {encoded, blocks};
                    },
                    quantTable
                ));
            }
            
            // 4. Pipeline с разным числом потоков
            for (int numThreads : {2, 4}) {
                results.push_back(runBenchmark(
                    "4. Pipeline (" + to_string(numThreads) + " threads)",
                    images,
                    [quality, numThreads](const RgbImage& img) -> EncodingResult {
                        vector<QuantizedBlock> blocks;
                        auto colorConv = make_unique<SequentialColorConverter>();
                        auto dct = make_unique<SequentialDctTransform>();
                        auto quant = make_unique<SequentialQuantizer>(quality);
                        auto innerProc = make_unique<PipelineBlockProcessor>(move(dct), move(quant), numThreads);
                        auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                        auto huffman = make_unique<SequentialHuffmanEncoder>();
                        JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                        auto encoded = encoder.encode(img);
                        return {encoded, blocks};
                    },
                    quantTable
                ));
            }
            
            // 5. Mix: MultiThread Color + OpenMP Blocks
            results.push_back(runBenchmark(
                "5. Mix: MT ColorConv(2) + OpenMP(4)",
                images,
                [quality](const RgbImage& img) -> EncodingResult {
                    omp_set_num_threads(4);
                    vector<QuantizedBlock> blocks;
                    auto colorConv = make_unique<MultiThreadColorConverter>(2);
                    auto dct = make_unique<OpenMPDctTransform>();
                    auto quant = make_unique<OpenMPQuantizer>(quality);
                    auto innerProc = make_unique<OpenMPBlockProcessor>(move(dct), move(quant));
                    auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    auto encoded = encoder.encode(img);
                    return {encoded, blocks};
                },
                quantTable
            ));
            
            // 6. Только MultiThread ColorConverter
            results.push_back(runBenchmark(
                "6. MT ColorConv(4) only",
                images,
                [quality](const RgbImage& img) -> EncodingResult {
                    vector<QuantizedBlock> blocks;
                    auto colorConv = make_unique<MultiThreadColorConverter>(4);
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(quality);
                    auto innerProc = make_unique<SequentialBlockProcessor>(move(dct), move(quant));
                    auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    auto encoded = encoder.encode(img);
                    return {encoded, blocks};
                },
                quantTable
            ));
            
            // 7. Только MultiThread BlockProcessor
            results.push_back(runBenchmark(
                "7. MT Blocks(4) only",
                images,
                [quality](const RgbImage& img) -> EncodingResult {
                    vector<QuantizedBlock> blocks;
                    auto colorConv = make_unique<SequentialColorConverter>();
                    auto dct = make_unique<SequentialDctTransform>();
                    auto quant = make_unique<SequentialQuantizer>(quality);
                    auto innerProc = make_unique<MultiThreadBlockProcessor>(move(dct), move(quant), 4);
                    auto blockProc = make_unique<BlockCapturingProcessor>(move(innerProc), &blocks);
                    auto huffman = make_unique<SequentialHuffmanEncoder>();
                    JpegEncoder encoder(move(colorConv), move(blockProc), move(huffman));
                    auto encoded = encoder.encode(img);
                    return {encoded, blocks};
                },
                quantTable
            ));
            
            printResults(results);
            
            // Проверка консистентности
            cout << "\n=== Quality Consistency Check ===" << endl;
            double baselinePsnr = results[0].avgPsnr;
            double baselineSsim = results[0].avgSsim;
            bool allConsistent = true;
            
            for (size_t i = 1; i < results.size(); i++) {
                double psnrDiff = abs(results[i].avgPsnr - baselinePsnr);
                double ssimDiff = abs(results[i].avgSsim - baselineSsim);
                
                bool consistent = (psnrDiff < 0.5 && ssimDiff < 0.01);
                
                if (!consistent) {
                    cout << results[i].name << ": MISMATCH! (PSNR diff: " << psnrDiff 
                         << ", SSIM diff: " << ssimDiff << ")" << endl;
                    allConsistent = false;
                }
            }
            
            if (allConsistent) {
                cout << "All parallel implementations produce consistent results!" << endl;
            }
            
            // Анализ
            cout << "\n=== Analysis ===" << endl;
            
            long long bestTime = results[0].avgTimeMs;
            string bestMethod = results[0].name;
            for (const auto& r : results) {
                if (r.avgTimeMs < bestTime) {
                    bestTime = r.avgTimeMs;
                    bestMethod = r.name;
                }
            }
            
            double bestSpeedup = static_cast<double>(results[0].avgTimeMs) / bestTime;
            cout << "Best method: " << bestMethod << " (" << fixed << setprecision(2) << bestSpeedup << "x speedup)" << endl;
            cout << "Theoretical max (Amdahl, 80% parallel): " 
                 << fixed << setprecision(2) << 1.0 / (0.2 + 0.8 / maxThreads) << "x" << endl;
            
        } catch (const exception& e) {
            cerr << "\nError during benchmark: " << e.what() << endl;
        }
    }
    
    cout << "\n" << string(124, '=') << endl;
    cout << "Benchmarks completed!" << endl;
    cout << string(124, '=') << endl;
    
    return 0;
}