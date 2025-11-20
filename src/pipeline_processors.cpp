// pipeline_processors.cpp
#include "pipeline_processor.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <future>

using namespace std;

// PipelineBlockProcessor
PipelineBlockProcessor::PipelineBlockProcessor(unique_ptr<IDctTransform> dctTransform, 
                                             unique_ptr<IQuantizer> quantizer)
    : dct(move(dctTransform)), quantizer(move(quantizer)) {}

vector<QuantizedBlock> PipelineBlockProcessor::processBlocks(const YCbCrImage& image) {
    // ?????????? ???????? ??? ?????????
    ProcessingPipeline pipeline(move(dct), move(quantizer), 6);
    return pipeline.processImage(image);
}

vector<vector<double>> PipelineBlockProcessor::extractBlock(const YCbCrImage& image, 
                                                           int x, int y, int component) {
    vector<vector<double>> block(8, vector<double>(8));
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int px = min(x + j, image.getWidth() - 1);
            int py = min(y + i, image.getHeight() - 1);
            auto pixel = image.getPixel(px, py);
            
            switch (component) {
                case 0: // Y component
                    block[i][j] = get<0>(pixel) - 128;
                    break;
                case 1: // Cb component  
                    block[i][j] = get<1>(pixel) - 128;
                    break;
                case 2: // Cr component
                    block[i][j] = get<2>(pixel) - 128;
                    break;
                default:
                    block[i][j] = 0;
            }
        }
    }
    
    return block;
}

// PipelineColorConverter
YCbCrImage PipelineColorConverter::convert(const RgbImage& image) {
    YCbCrImage result(image.getWidth(), image.getHeight());
    int height = image.getHeight();
    int width = image.getWidth();
    
    // ???????????? ??????????? ?????
    vector<future<void>> futures;
    int numThreads = thread::hardware_concurrency();
    int rowsPerThread = (height + numThreads - 1) / numThreads;
    
    for (int t = 0; t < numThreads; t++) {
        futures.push_back(async(launch::async, [&, t]() {
            int startRow = t * rowsPerThread;
            int endRow = min(startRow + rowsPerThread, height);
            
            for (int y = startRow; y < endRow; y++) {
                for (int x = 0; x < width; x++) {
                    auto rgb = image.getPixel(x, y);
                    auto ycbcr = ColorMath::rgbToYCbCr(get<0>(rgb), get<1>(rgb), get<2>(rgb));
                    result.setPixel(x, y, get<0>(ycbcr), get<1>(ycbcr), get<2>(ycbcr));
                }
            }
        }));
    }
    
    // ??????? ?????????? ???? ???????
    for (auto& future : futures) {
        future.get();
    }
    
    return result;
}

// PipelineDctTransform
vector<vector<double>> PipelineDctTransform::forwardDct(const vector<vector<double>>& block) {
    vector<vector<double>> result(8, vector<double>(8));
    
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            result[u][v] = DctMath::computeDctCoefficient(block, u, v);
        }
    }
    
    return result;
}

// PipelineHuffmanEncoder
JpegEncodedData PipelineHuffmanEncoder::encode(const vector<QuantizedBlock>& blocks, 
                                              int width, int height, 
                                              const vector<vector<int>>& quantTable) {
    if (blocks.empty()) {
        return JpegEncodedData{
            vector<unsigned char>(),
            unordered_map<int, pair<int, int>>(),
            unordered_map<int, pair<int, int>>(),
            quantTable,
            width,
            height
        };
    }
    
    // ????????? ????? ?? ???????????
    vector<QuantizedBlock> yBlocks, cbBlocks, crBlocks;
    for (const auto& block : blocks) {
        switch (block.getComponent()) {
            case 0: yBlocks.push_back(block); break;
            case 1: cbBlocks.push_back(block); break;
            case 2: crBlocks.push_back(block); break;
        }
    }
    
    // ???????????? ????????? ???????????
    auto yFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, yBlocks, "Y");
    auto cbFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, cbBlocks, "Cb");
    auto crFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, crBlocks, "Cr");
    
    auto yData = yFuture.get();
    auto cbData = cbFuture.get();
    auto crData = crFuture.get();
    
    // ???????? ??? ????? (последовательно, так как BitWriter не thread-safe)
    BitWriter writer;
    
    // ???????? Y ???????????
    for (const auto& block : yData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = yData.huffmanTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    // ???????? Cb ???????????
    for (const auto& block : cbData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = cbData.huffmanTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    // ???????? Cr ???????????
    for (const auto& block : crData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = crData.huffmanTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    return JpegEncodedData{
        writer.toArray(),
        yData.huffmanTable,
        yData.huffmanTable, // AC table ??? Y (?????????)
        quantTable,
        width,
        height
    };
}

PipelineHuffmanEncoder::ComponentData PipelineHuffmanEncoder::processComponent(
    const vector<QuantizedBlock>& blocks, const string& name) {
    
    ComponentData data;
    data.blocks = blocks;
    
    unordered_map<int, int> frequencies;
    for (const auto& block : blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            frequencies[coef]++;
        }
    }
    
    auto tree = HuffmanMath::buildTree(frequencies);
    data.huffmanTable = HuffmanMath::buildCodeTable(tree);
    delete tree;
    
    return data;
}

void PipelineHuffmanEncoder::encodeBlock(BitWriter& writer, const vector<int>& zigzag,
                                        const unordered_map<int, pair<int, int>>& dcTable,
                                        const unordered_map<int, pair<int, int>>& acTable) {
    // DC component
    int dc = zigzag[0];
    int dcDiff = dc - lastDc;
    lastDc = dc;
    
    int dcCategory = getCategory(dcDiff);
    auto dcCodeInfo = dcTable.at(dcCategory);
    writer.writeBits(dcCodeInfo.first, dcCodeInfo.second);
    
    if (dcCategory > 0) {
        int magnitude = getMagnitude(dcDiff, dcCategory);
        writer.writeBits(magnitude, dcCategory);
    }
    
    // AC components
    int zeroRun = 0;
    for (int i = 1; i < 64; i++) {
        int ac = zigzag[i];
        
        if (ac == 0) {
            zeroRun++;
            if (i == 63) { // EOB
                auto eobCodeInfo = acTable.at(0x00);
                writer.writeBits(eobCodeInfo.first, eobCodeInfo.second);
            }
        } else {
            while (zeroRun > 15) {
                auto zrlCodeInfo = acTable.at(0xF0);
                writer.writeBits(zrlCodeInfo.first, zrlCodeInfo.second);
                zeroRun -= 16;
            }
            
            int category = getCategory(ac);
            int symbol = (zeroRun << 4) | category;
            auto acCodeInfo = acTable.at(symbol);
            writer.writeBits(acCodeInfo.first, acCodeInfo.second);
            
            int magnitude = getMagnitude(ac, category);
            writer.writeBits(magnitude, category);
            
            zeroRun = 0;
        }
    }
}

int PipelineHuffmanEncoder::getCategory(int value) {
    int absValue = abs(value);
    if (absValue == 0) return 0;
    return static_cast<int>(floor(log2(absValue))) + 1;
}

int PipelineHuffmanEncoder::getMagnitude(int value, int category) {
    if (value >= 0)
        return value;
    return value + (1 << category) - 1;
}

// PipelineQuantizer
PipelineQuantizer::PipelineQuantizer(int quality) 
    : quantizationTable(generateQuantizationTable(quality)) {}

vector<vector<int>> PipelineQuantizer::quantize(const vector<vector<double>>& dctBlock) {
    vector<vector<int>> result(8, vector<int>(8));
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            result[i][j] = static_cast<int>(round(
                dctBlock[i][j] / quantizationTable[i][j]
            ));
        }
    }
    
    return result;
}

vector<vector<int>> PipelineQuantizer::defaultQuantizationTable() {
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

vector<vector<int>> PipelineQuantizer::generateQuantizationTable(int quality) {
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

// ProcessingPipeline
ProcessingPipeline::ProcessingPipeline(unique_ptr<IDctTransform> dctTransform,
                                     unique_ptr<IQuantizer> quantizerObj,
                                     int threadCount)
    : dct(move(dctTransform)), quantizer(move(quantizerObj)), numThreads(threadCount),
          ycbcrImage(nullptr) {}

ProcessingPipeline::~ProcessingPipeline() {
    // Устанавливаем флаги завершения
    dctFinished = true;
    quantizationFinished = true;
    
    // Будим все рабочие потоки
    dctCV.notify_all();
    quantizationCV.notify_all();
    
    // Ждем завершения всех потоков
    for (auto& thread : dctThreads) if (thread.joinable()) thread.join();
    for (auto& thread : quantizationThreads) if (thread.joinable()) thread.join();
}

vector<QuantizedBlock> ProcessingPipeline::processImage(const YCbCrImage& image) {
    ycbcrImage = &image;
    finalBlocks.clear();
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Запускаем DCT и квантизацию потоки
    for (int i = 0; i < numThreads; i++) {
        dctThreads.emplace_back(&ProcessingPipeline::dctStage, this);
        quantizationThreads.emplace_back(&ProcessingPipeline::quantizationStage, this);
    }
    
    // Извлекаем блоки параллельно и сразу кладем в DCT очередь
    vector<future<void>> extractionFutures;
    
    // Y компонент - полное разрешение
    extractionFutures.push_back(async(launch::async, [&]() {
        for (int by = 0; by < height; by += 8) {
            for (int bx = 0; bx < width; bx += 8) {
                // Извлекаем блок
                vector<vector<double>> extracted(8, vector<double>(8));
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        int px = min(bx + j, width - 1);
                        int py = min(by + i, height - 1);
                        auto pixel = image.getPixel(px, py);
                        extracted[i][j] = get<0>(pixel) - 128;
                    }
                }
                
                // Сразу применяем DCT и кладем в DCT очередь
                DctBlock dctBlock;
                dctBlock.x = bx;
                dctBlock.y = by;
                dctBlock.component = 0;
                dctBlock.dctCoeffs = dct->forwardDct(extracted);
                
                unique_lock<mutex> lock(dctMutex);
                dctQueue.push(move(dctBlock));
                lock.unlock();
                dctCV.notify_one();
            }
        }
    }));
    
    // Cb и Cr компоненты - субсэмплинг 2x2
    extractionFutures.push_back(async(launch::async, [&]() {
        for (int by = 0; by < height; by += 16) {
            for (int bx = 0; bx < width; bx += 16) {
                // Cb блок
                vector<vector<double>> cbExtracted(8, vector<double>(8));
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        int px = min(bx + j * 2, width - 1);
                        int py = min(by + i * 2, height - 1);
                        auto pixel = image.getPixel(px, py);
                        cbExtracted[i][j] = get<1>(pixel) - 128;
                    }
                }
                
                DctBlock cbDctBlock;
                cbDctBlock.x = bx;
                cbDctBlock.y = by;
                cbDctBlock.component = 1;
                cbDctBlock.dctCoeffs = dct->forwardDct(cbExtracted);
                
                unique_lock<mutex> lock(dctMutex);
                dctQueue.push(move(cbDctBlock));
                lock.unlock();
                dctCV.notify_one();
                
                // Cr блок
                vector<vector<double>> crExtracted(8, vector<double>(8));
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        int px = min(bx + j * 2, width - 1);
                        int py = min(by + i * 2, height - 1);
                        auto pixel = image.getPixel(px, py);
                        crExtracted[i][j] = get<2>(pixel) - 128;
                    }
                }
                
                DctBlock crDctBlock;
                crDctBlock.x = bx;
                crDctBlock.y = by;
                crDctBlock.component = 2;
                crDctBlock.dctCoeffs = dct->forwardDct(crExtracted);
                
                lock.lock();
                dctQueue.push(move(crDctBlock));
                lock.unlock();
                dctCV.notify_one();
            }
        }
    }));
    
    // Ждем завершения извлечения
    for (auto& future : extractionFutures) {
        future.get();
    }
    
    // Сигнализируем завершение DCT стадии
    dctFinished = true;
    dctCV.notify_all();
    
    // Ждем завершения всех потоков
    for (auto& thread : dctThreads) thread.join();
    for (auto& thread : quantizationThreads) thread.join();
    
    return finalBlocks;
}

void ProcessingPipeline::dctStage() {
    while (true) {
        unique_lock<mutex> lock(dctMutex);
        dctCV.wait(lock, [&]() { 
            return !dctQueue.empty() || dctFinished; 
        });
        
        if (dctQueue.empty() && dctFinished) {
            break;
        }
        
        if (!dctQueue.empty()) {
            DctBlock dctBlock = move(dctQueue.front());
            dctQueue.pop();
            lock.unlock();
            
            // ???????? ? ???????????
            QuantizedBlockData quantData;
            quantData.x = dctBlock.x;
            quantData.y = dctBlock.y;
            quantData.component = dctBlock.component;
            quantData.quantized = quantizer->quantize(dctBlock.dctCoeffs);
            
            unique_lock<mutex> quantLock(quantizationMutex);
            quantizationQueue.push(move(quantData));
            quantLock.unlock();
            quantizationCV.notify_one();
        }
    }
    
    // ???????? ??????????? ??? ???????????
    quantizationFinished = true;
    quantizationCV.notify_all();
}

void ProcessingPipeline::quantizationStage() {
    while (true) {
        unique_lock<mutex> lock(quantizationMutex);
        quantizationCV.wait(lock, [&]() { 
            return !quantizationQueue.empty() || quantizationFinished; 
        });
        
        if (quantizationQueue.empty() && quantizationFinished) {
            break;
        }
        
        if (!quantizationQueue.empty()) {
            QuantizedBlockData quantData = move(quantizationQueue.front());
            quantizationQueue.pop();
            lock.unlock();
            
            // ??????? ????????? ????
            int blockX = quantData.component == 0 ? quantData.x / 8 : quantData.x / 16;
            int blockY = quantData.component == 0 ? quantData.y / 8 : quantData.y / 16;
            
            QuantizedBlock finalBlock(quantData.quantized, blockX, blockY, quantData.component);
            
            unique_lock<mutex> finalLock(finalMutex);
            finalBlocks.push_back(move(finalBlock));
        }
    }
}

// PipelineJpegEncoder
PipelineJpegEncoder::PipelineJpegEncoder(unique_ptr<IColorConverter> colorConv,
                                       unique_ptr<IDctTransform> dctTransform,
                                       unique_ptr<IQuantizer> quantizer,
                                       unique_ptr<IHuffmanEncoder> huffmanEnc,
                                       int threadCount)
    : colorConverter(move(colorConv)),
      pipeline(make_unique<ProcessingPipeline>(move(dctTransform), move(quantizer), threadCount)),
      encoder(move(huffmanEnc)) {}

JpegEncodedData PipelineJpegEncoder::encode(const RgbImage& image) {
    auto ycbcr = colorConverter->convert(image);
    auto blocks = pipeline->processImage(ycbcr);
    auto quantTable = PipelineQuantizer::defaultQuantizationTable();
    
    return encoder->encode(blocks, image.getWidth(), image.getHeight(), quantTable);
}