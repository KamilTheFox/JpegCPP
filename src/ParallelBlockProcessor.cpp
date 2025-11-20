#include "ParallelBlockProcessor.h"
#include <algorithm>

using namespace std;

ParallelBlockProcessor::ParallelBlockProcessor(unique_ptr<ParallelDctTransform> dctTransform, 
                                             unique_ptr<ParallelQuantizer> quantizer,
                                             int threadCount)
    : dct(move(dctTransform)), quantizer(move(quantizer)) {}

vector<QuantizedBlock> ParallelBlockProcessor::processBlocks(const YCbCrImage& image) {
    vector<QuantizedBlock> blocks;
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Собираем все блоки для пакетной обработки
    vector<vector<vector<double>>> yBlocks, cbBlocks, crBlocks;
    vector<tuple<int, int, int>> blockInfo; // (x, y, component)
    
    // Y компоненты
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            yBlocks.push_back(extractBlock(image, bx, by, 0));
            blockInfo.emplace_back(bx / 8, by / 8, 0);
        }
    }
    
    // Cb компоненты
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            cbBlocks.push_back(extractBlock(image, bx, by, 1));
            blockInfo.emplace_back(bx / 16, by / 16, 1);
        }
    }
    
    // Cr компоненты
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            crBlocks.push_back(extractBlock(image, bx, by, 2));
            blockInfo.emplace_back(bx / 16, by / 16, 2);
        }
    }
    
    // Параллельная обработка DCT
    auto dctFutures = dct->forwardDctBatch(yBlocks);
    for (auto& block : cbBlocks) {
        dctFutures.push_back(async(launch::async, [this, block]() {
            return dct->forwardDct(block);
        }));
    }
    for (auto& block : crBlocks) {
        dctFutures.push_back(async(launch::async, [this, block]() {
            return dct->forwardDct(block);
        }));
    }
    
    // Собираем результаты DCT
    vector<vector<vector<double>>> allDctBlocks;
    for (auto& future : dctFutures) {
        allDctBlocks.push_back(future.get());
    }
    
    // Параллельное квантование
    auto quantFutures = quantizer->quantizeBatch(allDctBlocks);
    
    // Собираем финальные блоки
    for (size_t i = 0; i < quantFutures.size(); i++) {
        auto quantized = quantFutures[i].get();
        auto [bx, by, comp] = blockInfo[i];
        blocks.emplace_back(quantized, bx, by, comp);
    }
    
    return blocks;
}

vector<future<QuantizedBlock>> ParallelBlockProcessor::processBlocksAsync(const YCbCrImage& image) {
    vector<future<QuantizedBlock>> futures;
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Асинхронная обработка каждого блока
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            futures.push_back(async(launch::async, [this, &image, bx, by]() {
                auto yBlock = extractBlock(image, bx, by, 0);
                auto yDct = dct->forwardDct(yBlock);
                auto yQuant = quantizer->quantize(yDct);
                return QuantizedBlock(yQuant, bx / 8, by / 8, 0);
            }));
        }
    }
    
    return futures;
}

vector<vector<double>> ParallelBlockProcessor::extractBlock(const YCbCrImage& image, 
                                                           int x, int y, int component) {
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