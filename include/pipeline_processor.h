#ifndef PIPELINE_PROCESSOR_H
#define PIPELINE_PROCESSOR_H

#include "interfaces.h"
#include <vector>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

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

// Настоящий конвейерный процессор блоков
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

#endif