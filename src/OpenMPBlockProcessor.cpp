#include "OpenMPBlockProcessor.h"
#include <omp.h>

using namespace std;

OpenMPBlockProcessor::OpenMPBlockProcessor(unique_ptr<OpenMPDctTransform> dctTransform, 
                                         unique_ptr<OpenMPQuantizer> quantizer)
    : dct(move(dctTransform)), quantizer(move(quantizer)) {}

vector<QuantizedBlock> OpenMPBlockProcessor::processBlocks(const YCbCrImage& image) {
    vector<QuantizedBlock> blocks;
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Собираем все блоки для обработки
    vector<vector<vector<double>>> allBlocks;
    vector<tuple<int, int, int>> blockInfo; // (x, y, component)
    
    // Y компоненты
    #pragma omp parallel for collapse(2)
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            #pragma omp critical
            {
                allBlocks.push_back(extractBlock(image, bx, by, 0));
                blockInfo.emplace_back(bx / 8, by / 8, 0);
            }
        }
    }
    
    // Cb компоненты
    #pragma omp parallel for collapse(2)
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            #pragma omp critical
            {
                allBlocks.push_back(extractBlock(image, bx, by, 1));
                blockInfo.emplace_back(bx / 16, by / 16, 1);
            }
        }
    }
    
    // Cr компоненты
    #pragma omp parallel for collapse(2)
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            #pragma omp critical
            {
                allBlocks.push_back(extractBlock(image, bx, by, 2));
                blockInfo.emplace_back(bx / 16, by / 16, 2);
            }
        }
    }
    
    // Параллельная обработка DCT
    auto dctResults = OpenMPDctTransform::forwardDctBatch(allBlocks);
    
    // Параллельное квантование
    auto quantResults = OpenMPQuantizer::quantizeBatch(dctResults, quantizer->getQuantizationTable());
    
    // Собираем результаты
    blocks.reserve(quantResults.size());
    for (size_t i = 0; i < quantResults.size(); i++) {
        auto [bx, by, comp] = blockInfo[i];
        blocks.emplace_back(quantResults[i], bx, by, comp);
    }
    
    return blocks;
}

vector<vector<double>> OpenMPBlockProcessor::extractBlock(const YCbCrImage& image, 
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