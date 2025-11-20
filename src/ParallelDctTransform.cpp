#include "ParallelDctTransform.h"
#include "dct_math.h"
#include <iostream>

using namespace std;

ParallelDctTransform::ParallelDctTransform(int threadCount) 
    : numThreads(threadCount > 0 ? threadCount : 1) {
    
    workers.reserve(numThreads);
    for (int i = 0; i < numThreads; i++) {
        workers.emplace_back(&ParallelDctTransform::workerThread, this);
    }
}

ParallelDctTransform::~ParallelDctTransform() {
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

void ParallelDctTransform::workerThread() {
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
            
            // Выполняем DCT преобразование
            vector<vector<double>> result(8, vector<double>(8));
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    result[u][v] = DctMath::computeDctCoefficient(task.inputBlock, u, v);
                }
            }
            
            task.promise.set_value(move(result));
        }
    }
}

vector<vector<double>> ParallelDctTransform::forwardDct(const vector<vector<double>>& block) {
    promise<vector<vector<double>>> promise;
    auto future = promise.get_future();
    
    {
        lock_guard<mutex> lock(queueMutex);
        taskQueue.push({block, move(promise)});
    }
    queueCV.notify_one();
    
    return future.get();
}

vector<future<vector<vector<double>>>> 
ParallelDctTransform::forwardDctBatch(const vector<vector<vector<double>>>& blocks) {
    vector<future<vector<vector<double>>>> futures;
    futures.reserve(blocks.size());
    
    {
        lock_guard<mutex> lock(queueMutex);
        for (const auto& block : blocks) {
            promise<vector<vector<double>>> promise;
            futures.push_back(promise.get_future());
            taskQueue.push({block, move(promise)});
        }
    }
    queueCV.notify_all();
    
    return futures;
}