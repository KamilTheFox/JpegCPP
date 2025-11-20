#include "pipeline_processor.h"
#include <algorithm>

using namespace std;

PipelineBlockProcessor::PipelineBlockProcessor(unique_ptr<IDctTransform> dctTransform,
                                             unique_ptr<IQuantizer> quantizer,
                                             int numThreads)
    : dct(move(dctTransform)), quantizer(move(quantizer)), numThreads(numThreads) {}

PipelineBlockProcessor::~PipelineBlockProcessor() {
    // Установим флаги завершения
    extractionDone = true;
    dctDone = true;
    quantDone = true;
    
    // Разбудим все потоки
    extractCV.notify_all();
    dctCV.notify_all();
    quantCV.notify_all();
    
    // Дождемся завершения
    for (auto& t : extractThreads) if (t.joinable()) t.join();
    for (auto& t : dctThreads) if (t.joinable()) t.join();
    for (auto& t : quantThreads) if (t.joinable()) t.join();
}

vector<vector<double>> PipelineBlockProcessor::extractBlock(
    const YCbCrImage& image, int x, int y, int component) {
    
    vector<vector<double>> block(8, vector<double>(8));
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int px = min(x + j, image.getWidth() - 1);
            int py = min(y + i, image.getHeight() - 1);
            auto pixel = image.getPixel(px, py);
            
            switch (component) {
                case 0: block[i][j] = get<0>(pixel) - 128; break;
                case 1: block[i][j] = get<1>(pixel) - 128; break;
                case 2: block[i][j] = get<2>(pixel) - 128; break;
                default: block[i][j] = 0;
            }
        }
    }
    
    return block;
}

void PipelineBlockProcessor::extractionStage(const YCbCrImage& image) {
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Y компоненты
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            RawBlock block;
            block.data = extractBlock(image, bx, by, 0);
            block.x = bx / 8;
            block.y = by / 8;
            block.component = 0;
            
            unique_lock<mutex> lock(extractMutex);
            extractQueue.push(move(block));
            lock.unlock();
            extractCV.notify_one();
        }
    }
    
    // Cb компоненты
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            RawBlock block;
            block.data = extractBlock(image, bx, by, 1);
            block.x = bx / 16;
            block.y = by / 16;
            block.component = 1;
            
            unique_lock<mutex> lock(extractMutex);
            extractQueue.push(move(block));
            lock.unlock();
            extractCV.notify_one();
        }
    }
    
    // Cr компоненты
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            RawBlock block;
            block.data = extractBlock(image, bx, by, 2);
            block.x = bx / 16;
            block.y = by / 16;
            block.component = 2;
            
            unique_lock<mutex> lock(extractMutex);
            extractQueue.push(move(block));
            lock.unlock();
            extractCV.notify_one();
        }
    }
    
    extractionDone = true;
    extractCV.notify_all();
}

void PipelineBlockProcessor::dctStage() {
    while (true) {
        unique_lock<mutex> lock(extractMutex);
        extractCV.wait(lock, [&]() {
            return !extractQueue.empty() || extractionDone;
        });
        
        if (extractQueue.empty() && extractionDone) {
            break;
        }
        
        if (!extractQueue.empty()) {
            RawBlock raw = move(extractQueue.front());
            extractQueue.pop();
            lock.unlock();
            
            // Применяем DCT
            DctBlock dctBlock;
            dctBlock.dctCoeffs = dct->forwardDct(raw.data);
            dctBlock.x = raw.x;
            dctBlock.y = raw.y;
            dctBlock.component = raw.component;
            
            unique_lock<mutex> dctLock(dctMutex);
            dctQueue.push(move(dctBlock));
            dctLock.unlock();
            dctCV.notify_one();
        }
    }
    
    dctDone = true;
    dctCV.notify_all();
}

void PipelineBlockProcessor::quantizationStage() {
    while (true) {
        unique_lock<mutex> lock(dctMutex);
        dctCV.wait(lock, [&]() {
            return !dctQueue.empty() || dctDone;
        });
        
        if (dctQueue.empty() && dctDone) {
            break;
        }
        
        if (!dctQueue.empty()) {
            DctBlock dctBlock = move(dctQueue.front());
            dctQueue.pop();
            lock.unlock();
            
            // Применяем квантование
            QuantizedBlockData quantData;
            quantData.quantized = quantizer->quantize(dctBlock.dctCoeffs);
            quantData.x = dctBlock.x;
            quantData.y = dctBlock.y;
            quantData.component = dctBlock.component;
            
            // Создаем финальный блок
            QuantizedBlock finalBlock(quantData.quantized, quantData.x, quantData.y, quantData.component);
            
            unique_lock<mutex> finalLock(finalMutex);
            finalBlocks.push_back(move(finalBlock));
        }
    }
    
    quantDone = true;
}

vector<QuantizedBlock> PipelineBlockProcessor::processBlocks(const YCbCrImage& image) {
    // Сброс состояния
    extractionDone = false;
    dctDone = false;
    quantDone = false;
    
    // Очистка очередей
    while (!extractQueue.empty()) extractQueue.pop();
    while (!dctQueue.empty()) dctQueue.pop();
    while (!quantQueue.empty()) quantQueue.pop();
    finalBlocks.clear();
    
    // Запуск стадий конвейера
    // Extraction - 1 поток (главный)
    thread extractThread(&PipelineBlockProcessor::extractionStage, this, ref(image));
    
    // DCT - несколько потоков
    int dctThreadCount = max(1, numThreads / 2);
    for (int i = 0; i < dctThreadCount; i++) {
        dctThreads.emplace_back(&PipelineBlockProcessor::dctStage, this);
    }
    
    // Quantization - несколько потоков
    int quantThreadCount = max(1, numThreads / 2);
    for (int i = 0; i < quantThreadCount; i++) {
        quantThreads.emplace_back(&PipelineBlockProcessor::quantizationStage, this);
    }
    
    // Ждем завершения
    extractThread.join();
    for (auto& t : dctThreads) t.join();
    for (auto& t : quantThreads) t.join();
    
    dctThreads.clear();
    quantThreads.clear();
    
    return finalBlocks;
}