#ifndef PIPELINE_PROCESSOR_H
#define PIPELINE_PROCESSOR_H

#include "interfaces.h"
#include "dct_math.h"
#include "color_math.h"
#include "huffman_math.h"
#include "bit_writer.h"
#include <vector>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <unordered_map>

using namespace std;

// Структуры для передачи данных между стадиями конвейера
struct RawBlock {
    vector<vector<double>> data;
    int x, y, component;
};

struct DctBlock {
    vector<vector<double>> dctCoeffs;
    int x, y, component;
};

struct QuantizedBlockData {
    vector<vector<int>> quantized;
    int x, y, component;
};

// ========== Producer-Consumer Pipeline BlockProcessor ==========
class PipelineBlockProcessor : public IBlockProcessor {
private:
    unique_ptr<IDctTransform> dct;
    unique_ptr<IQuantizer> quantizer;
    int numThreads;
    
    // Очереди для конвейера
    queue<RawBlock> extractQueue;
    queue<DctBlock> dctQueue;
    queue<QuantizedBlockData> quantQueue;
    vector<QuantizedBlock> finalBlocks;
    
    // Синхронизация
    mutex extractMutex, dctMutex, quantMutex, finalMutex;
    condition_variable extractCV, dctCV, quantCV;
    atomic<bool> extractionDone{false};
    atomic<bool> dctDone{false};
    atomic<bool> quantDone{false};
    
    // Потоки конвейера
    vector<thread> extractThreads;
    vector<thread> dctThreads;
    vector<thread> quantThreads;
    
    // Стадии конвейера
    void extractionStage(const YCbCrImage& image);
    void dctStage();
    void quantizationStage();
    
    static vector<vector<double>> extractBlock(const YCbCrImage& image, int x, int y, int component);

public:
    PipelineBlockProcessor(unique_ptr<IDctTransform> dctTransform, 
                          unique_ptr<IQuantizer> quantizer,
                          int numThreads = thread::hardware_concurrency());
    ~PipelineBlockProcessor();
    
    vector<QuantizedBlock> processBlocks(const YCbCrImage& image) override;
};

// ========== Async Pipeline компоненты ==========

// Конвейерный конвертер цветов
class PipelineColorConverter : public IColorConverter {
public:
    YCbCrImage convert(const RgbImage& image) override;
};

// Конвейерный DCT
class PipelineDctTransform : public IDctTransform {
public:
    vector<vector<double>> forwardDct(const vector<vector<double>>& block) override;
};

// Конвейерный квантователь
class PipelineQuantizer : public IQuantizer {
private:
    vector<vector<int>> quantizationTable;
    static vector<vector<int>> generateQuantizationTable(int quality);

public:
    PipelineQuantizer(int quality = 50);
    vector<vector<int>> quantize(const vector<vector<double>>& dctBlock) override;
    
    static vector<vector<int>> defaultQuantizationTable();
    const vector<vector<int>>& getQuantizationTable() const { return quantizationTable; }
};

// Конвейерный Huffman encoder
class PipelineHuffmanEncoder : public IHuffmanEncoder {
private:
    struct ComponentData {
        vector<QuantizedBlock> blocks;
        unordered_map<int, pair<int, int>> huffmanTable;
    };
    
    int lastDc = 0;
    
    void encodeBlock(BitWriter& writer, const vector<int>& zigzag,
                    const unordered_map<int, pair<int, int>>& dcTable,
                    const unordered_map<int, pair<int, int>>& acTable);
    
    static int getCategory(int value);
    static int getMagnitude(int value, int category);
    
    ComponentData processComponent(const vector<QuantizedBlock>& blocks, const string& name);

public:
    JpegEncodedData encode(const vector<QuantizedBlock>& blocks, 
                          int width, int height, 
                          const vector<vector<int>>& quantTable) override;
};

// Вспомогательный конвейер обработки (async-based)
class ProcessingPipeline {
private:
    const YCbCrImage* ycbcrImage;
    vector<QuantizedBlock> finalBlocks;
    
    // Очереди для передачи данных между стадиями
    queue<DctBlock> dctQueue;
    queue<QuantizedBlockData> quantizationQueue;
    
    // Мьютексы и условные переменные
    mutex dctMutex, quantizationMutex, finalMutex;
    condition_variable dctCV, quantizationCV;
    atomic<bool> dctFinished{false}, quantizationFinished{false};
    
    // Потоки
    vector<thread> dctThreads;
    vector<thread> quantizationThreads;
    
    unique_ptr<IDctTransform> dct;
    unique_ptr<IQuantizer> quantizer;
    int numThreads;

    void dctStage();
    void quantizationStage();

public:
    ProcessingPipeline(unique_ptr<IDctTransform> dctTransform, 
                      unique_ptr<IQuantizer> quantizerObj, 
                      int threadCount);
    ~ProcessingPipeline();
    
    vector<QuantizedBlock> processImage(const YCbCrImage& image);
};

// Высокоуровневый Pipeline JPEG encoder
class PipelineJpegEncoder {
private:
    unique_ptr<IColorConverter> colorConverter;
    unique_ptr<ProcessingPipeline> pipeline;
    unique_ptr<IHuffmanEncoder> encoder;

public:
    PipelineJpegEncoder(unique_ptr<IColorConverter> colorConv,
                       unique_ptr<IDctTransform> dctTransform,
                       unique_ptr<IQuantizer> quantizer,
                       unique_ptr<IHuffmanEncoder> huffmanEnc,
                       int threadCount = thread::hardware_concurrency());
    
    JpegEncodedData encode(const RgbImage& image);
};

#endif
