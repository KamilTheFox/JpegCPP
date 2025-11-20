#ifndef PARALLEL_DCT_TRANSFORM_H
#define PARALLEL_DCT_TRANSFORM_H

#include "interfaces.h"
#include <vector>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

class ParallelDctTransform : public IDctTransform {
private:
    int numThreads;
    
    struct DctTask {
        std::vector<std::vector<double>> inputBlock;
        std::promise<std::vector<std::vector<double>>> promise;
    };
    
    std::queue<DctTask> taskQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::vector<std::thread> workers;
    bool stopFlag = false;
    
    void workerThread();

public:
    ParallelDctTransform(int threadCount = std::thread::hardware_concurrency());
    ~ParallelDctTransform();
    
    std::vector<std::vector<double>> forwardDct(const std::vector<std::vector<double>>& block) override;
    
    // Пакетная обработка для pipeline
    std::vector<std::future<std::vector<std::vector<double>>>> 
    forwardDctBatch(const std::vector<std::vector<std::vector<double>>>& blocks);
};

#endif