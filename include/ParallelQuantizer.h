#ifndef PARALLEL_QUANTIZER_H
#define PARALLEL_QUANTIZER_H

#include "interfaces.h"
#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

class ParallelQuantizer : public IQuantizer {
private:
    std::vector<std::vector<int>> quantizationTable;
    int numThreads;
    
    struct QuantizeTask {
        std::vector<std::vector<double>> dctBlock;
        std::promise<std::vector<std::vector<int>>> promise;
    };
    
    std::queue<QuantizeTask> taskQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::vector<std::thread> workers;
    bool stopFlag = false;
    
    void workerThread();

public:
    ParallelQuantizer(int quality = 50, int threadCount = std::thread::hardware_concurrency());
    ~ParallelQuantizer();
    
    std::vector<std::vector<int>> quantize(const std::vector<std::vector<double>>& dctBlock) override;
    
    // Пакетная обработка для pipeline
    std::vector<std::future<std::vector<std::vector<int>>>> 
    quantizeBatch(const std::vector<std::vector<std::vector<double>>>& dctBlocks);
    
    static std::vector<std::vector<int>> defaultQuantizationTable();
    vector<vector<int>> generateQuantizationTable(int quality);
};

#endif