#include "ParallelQuantizer.h"
#include <algorithm>
#include <iostream>

using namespace std;

ParallelQuantizer::ParallelQuantizer(int quality, int threadCount) 
    : numThreads(threadCount > 0 ? threadCount : 1),
      quantizationTable(generateQuantizationTable(quality)) {
    
    workers.reserve(numThreads);
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back(&ParallelQuantizer::workerThread, this);
    }
}

ParallelQuantizer::~ParallelQuantizer() {
    {
        lock_guard<mutex> lock(queueMutex);
        stopFlag = true;
    }
    queueCV.notify_all();
    
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ParallelQuantizer::workerThread() {
    while (true) {
        unique_lock<mutex> lock(queueMutex);
        queueCV.wait(lock, [this]() { return stopFlag || !taskQueue.empty(); });
        
        if (stopFlag && taskQueue.empty()) {
            return;
        }
        
        if (!taskQueue.empty()) {
            auto task = move(taskQueue.front());
            taskQueue.pop();
            lock.unlock();
            
            // Выполняем квантование
            vector<vector<int>> result(8, vector<int>(8));
            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    result[i][j] = static_cast<int>(round(
                        task.dctBlock[i][j] / quantizationTable[i][j]
                    ));
                }
            }
            
            task.promise.set_value(move(result));
        }
    }
}

vector<vector<int>> ParallelQuantizer::quantize(const vector<vector<double>>& dctBlock) {
    promise<vector<vector<int>>> promise;
    auto future = promise.get_future();
    
    {
        lock_guard<mutex> lock(queueMutex);
        taskQueue.push({dctBlock, move(promise)});
    }
    queueCV.notify_one();
    
    return future.get();
}

vector<future<vector<vector<int>>>> 
ParallelQuantizer::quantizeBatch(const vector<vector<vector<double>>>& dctBlocks) {
    vector<future<vector<vector<int>>>> futures;
    futures.reserve(dctBlocks.size());
    
    {
        lock_guard<mutex> lock(queueMutex);
        for (const auto& block : dctBlocks) {
            promise<vector<vector<int>>> promise;
            futures.push_back(promise.get_future());
            taskQueue.push({block, move(promise)});
        }
    }
    queueCV.notify_all();
    
    return futures;
}

vector<vector<int>> ParallelQuantizer::defaultQuantizationTable() {
    return vector<vector<int>>{
        {16, 11, 10, 16, 24, 40, 51, 61},
        {12, 12, 14, 19, 26, 58, 60, 55},
        {14, 13, 16, 24, 40, 57, 69, 56},
        {14, 17, 22, 29, 51, 87, 80, 62},
        {18, 22, 37, 56, 68, 109, 103, 77},
        {24, 35, 55, 64, 81, 104, 113, 92},
        {49, 64, 78, 87, 103, 121, 120, 101},
        {72, 92, 95, 98, 112, 100, 103, 99}
    };
}

vector<vector<int>> ParallelQuantizer::generateQuantizationTable(int quality) {
    auto baseTable = defaultQuantizationTable();
    vector<vector<int>> result(8, vector<int>(8));
    
    double scale = quality < 50 ? 5000.0 / quality : 200.0 - 2 * quality;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int value = static_cast<int>((baseTable[i][j] * scale + 50) / 100);
            result[i][j] = max(1, min(255, value));
        }
    }
    
    return result;
}