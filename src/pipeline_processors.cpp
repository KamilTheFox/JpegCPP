#include "pipeline_processor.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

// ========== PipelineBlockProcessor (Producer-Consumer) ==========

PipelineBlockProcessor::PipelineBlockProcessor(unique_ptr<IDctTransform> dctTransform,
                                             unique_ptr<IQuantizer> quantizer,
                                             int numThreads)
    : dct(move(dctTransform)), quantizer(move(quantizer)), numThreads(numThreads) {}

PipelineBlockProcessor::~PipelineBlockProcessor() {
    extractionDone = true;
    dctDone = true;
    quantDone = true;
    
    extractCV.notify_all();
    dctCV.notify_all();
    quantCV.notify_all();
    
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
            
            QuantizedBlockData quantData;
            quantData.quantized = quantizer->quantize(dctBlock.dctCoeffs);
            quantData.x = dctBlock.x;
            quantData.y = dctBlock.y;
            quantData.component = dctBlock.component;
            
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
    
    while (!extractQueue.empty()) extractQueue.pop();
    while (!dctQueue.empty()) dctQueue.pop();
    while (!quantQueue.empty()) quantQueue.pop();
    finalBlocks.clear();
    
    // Запуск стадий конвейера
    thread extractThread(&PipelineBlockProcessor::extractionStage, this, ref(image));
    
    int dctThreadCount = max(1, numThreads / 2);
    for (int i = 0; i < dctThreadCount; i++) {
        dctThreads.emplace_back(&PipelineBlockProcessor::dctStage, this);
    }
    
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

// ========== PipelineColorConverter ==========

YCbCrImage PipelineColorConverter::convert(const RgbImage& image) {
    YCbCrImage result(image.getWidth(), image.getHeight());
    int height = image.getHeight();
    int width = image.getWidth();
    
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
    
    for (auto& future : futures) {
        future.get();
    }
    
    return result;
}

// ========== PipelineDctTransform ==========

vector<vector<double>> PipelineDctTransform::forwardDct(const vector<vector<double>>& block) {
    vector<vector<double>> result(8, vector<double>(8));
    
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            result[u][v] = DctMath::computeDctCoefficient(block, u, v);
        }
    }
    
    return result;
}

// ========== PipelineQuantizer ==========

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

// ========== PipelineHuffmanEncoder ==========

JpegEncodedData PipelineHuffmanEncoder::encode(const vector<QuantizedBlock>& blocks, 
                                              int width, int height, 
                                              const vector<vector<int>>& quantTable) {
    if (blocks.empty()) {
        JpegEncodedData result;
        result.compressedData = vector<unsigned char>();
        result.quantizationTable = quantTable;
        result.width = width;
        result.height = height;
        return result;
    }
    
    // Разделяем блоки по компонентам
    vector<QuantizedBlock> yBlocks, cbBlocks, crBlocks;
    for (const auto& block : blocks) {
        switch (block.getComponent()) {
            case 0: yBlocks.push_back(block); break;
            case 1: cbBlocks.push_back(block); break;
            case 2: crBlocks.push_back(block); break;
        }
    }
    
    // Параллельная обработка компонентов
    auto yFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, yBlocks, "Y");
    auto cbFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, cbBlocks, "Cb");
    auto crFuture = async(launch::async, &PipelineHuffmanEncoder::processComponent, this, crBlocks, "Cr");
    
    auto yData = yFuture.get();
    auto cbData = cbFuture.get();
    auto crData = crFuture.get();
    
    // Кодируем все блоки (последовательно, BitWriter не thread-safe)
    BitWriter writer;
    
    for (const auto& block : yData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto it = yData.huffmanTable.find(coef);
            if (it != yData.huffmanTable.end()) {
                writer.writeBits(it->second.first, it->second.second);
            }
        }
    }
    
    for (const auto& block : cbData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto it = cbData.huffmanTable.find(coef);
            if (it != cbData.huffmanTable.end()) {
                writer.writeBits(it->second.first, it->second.second);
            }
        }
    }
    
    for (const auto& block : crData.blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto it = crData.huffmanTable.find(coef);
            if (it != crData.huffmanTable.end()) {
                writer.writeBits(it->second.first, it->second.second);
            }
        }
    }
    
    JpegEncodedData result;
    result.compressedData = writer.toArray();
    result.yHuffmanTable = yData.huffmanTable;
    result.cbHuffmanTable = cbData.huffmanTable;
    result.crHuffmanTable = crData.huffmanTable;
    result.dcLuminanceTable = yData.huffmanTable;
    result.acLuminanceTable = yData.huffmanTable;
    result.quantizationTable = quantTable;
    result.width = width;
    result.height = height;
    result.yBlockCount = static_cast<int>(yData.blocks.size());
    result.cbBlockCount = static_cast<int>(cbData.blocks.size());
    result.crBlockCount = static_cast<int>(crData.blocks.size());
    return result;
}

PipelineHuffmanEncoder::ComponentData PipelineHuffmanEncoder::processComponent(
    const vector<QuantizedBlock>& blocks, const string& name) {
    
    ComponentData data;
    data.blocks = blocks;
    
    if (blocks.empty()) {
        return data;
    }
    
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
    int dc = zigzag[0];
    int dcDiff = dc - lastDc;
    lastDc = dc;
    
    int dcCategory = getCategory(dcDiff);
    auto dcIt = dcTable.find(dcCategory);
    if (dcIt != dcTable.end()) {
        writer.writeBits(dcIt->second.first, dcIt->second.second);
    }
    
    if (dcCategory > 0) {
        int magnitude = getMagnitude(dcDiff, dcCategory);
        writer.writeBits(magnitude, dcCategory);
    }
    
    int zeroRun = 0;
    for (int i = 1; i < 64; i++) {
        int ac = zigzag[i];
        
        if (ac == 0) {
            zeroRun++;
            if (i == 63) {
                auto eobIt = acTable.find(0x00);
                if (eobIt != acTable.end()) {
                    writer.writeBits(eobIt->second.first, eobIt->second.second);
                }
            }
        } else {
            while (zeroRun > 15) {
                auto zrlIt = acTable.find(0xF0);
                if (zrlIt != acTable.end()) {
                    writer.writeBits(zrlIt->second.first, zrlIt->second.second);
                }
                zeroRun -= 16;
            }
            
            int category = getCategory(ac);
            int symbol = (zeroRun << 4) | category;
            auto acIt = acTable.find(symbol);
            if (acIt != acTable.end()) {
                writer.writeBits(acIt->second.first, acIt->second.second);
            }
            
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

// ========== ProcessingPipeline ==========

ProcessingPipeline::ProcessingPipeline(unique_ptr<IDctTransform> dctTransform,
                                     unique_ptr<IQuantizer> quantizerObj,
                                     int threadCount)
    : dct(move(dctTransform)), quantizer(move(quantizerObj)), numThreads(threadCount),
      ycbcrImage(nullptr) {}

ProcessingPipeline::~ProcessingPipeline() {
    dctFinished = true;
    quantizationFinished = true;
    
    dctCV.notify_all();
    quantizationCV.notify_all();
    
    for (auto& thread : dctThreads) if (thread.joinable()) thread.join();
    for (auto& thread : quantizationThreads) if (thread.joinable()) thread.join();
}

vector<QuantizedBlock> ProcessingPipeline::processImage(const YCbCrImage& image) {
    ycbcrImage = &image;
    finalBlocks.clear();
    
    // Сброс состояния
    dctFinished = false;
    quantizationFinished = false;
    while (!dctQueue.empty()) dctQueue.pop();
    while (!quantizationQueue.empty()) quantizationQueue.pop();
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Запускаем квантизацию потоки
    for (int i = 0; i < numThreads; i++) {
        quantizationThreads.emplace_back(&ProcessingPipeline::quantizationStage, this);
    }
    
    // Извлекаем блоки параллельно и сразу кладем в DCT очередь
    vector<future<void>> extractionFutures;
    
    // Y компонент - полное разрешение
    extractionFutures.push_back(async(launch::async, [&]() {
        for (int by = 0; by < height; by += 8) {
            for (int bx = 0; bx < width; bx += 8) {
                vector<vector<double>> extracted(8, vector<double>(8));
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        int px = min(bx + j, width - 1);
                        int py = min(by + i, height - 1);
                        auto pixel = image.getPixel(px, py);
                        extracted[i][j] = get<0>(pixel) - 128;
                    }
                }
                
                // DCT
                auto dctCoeffs = dct->forwardDct(extracted);
                
                DctBlock dctBlock;
                dctBlock.x = bx / 8;
                dctBlock.y = by / 8;
                dctBlock.component = 0;
                dctBlock.dctCoeffs = move(dctCoeffs);
                
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
                
                auto cbDctCoeffs = dct->forwardDct(cbExtracted);
                
                DctBlock cbDctBlock;
                cbDctBlock.x = bx / 16;
                cbDctBlock.y = by / 16;
                cbDctBlock.component = 1;
                cbDctBlock.dctCoeffs = move(cbDctCoeffs);
                
                {
                    unique_lock<mutex> lock(dctMutex);
                    dctQueue.push(move(cbDctBlock));
                }
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
                
                auto crDctCoeffs = dct->forwardDct(crExtracted);
                
                DctBlock crDctBlock;
                crDctBlock.x = bx / 16;
                crDctBlock.y = by / 16;
                crDctBlock.component = 2;
                crDctBlock.dctCoeffs = move(crDctCoeffs);
                
                {
                    unique_lock<mutex> lock(dctMutex);
                    dctQueue.push(move(crDctBlock));
                }
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
    for (auto& thread : quantizationThreads) thread.join();
    
    quantizationThreads.clear();
    dctThreads.clear();
    
    return finalBlocks;
}

void ProcessingPipeline::dctStage() {
    // В текущей реализации DCT выполняется в extraction futures
}

void ProcessingPipeline::quantizationStage() {
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
            
            // Квантизация
            auto quantized = quantizer->quantize(dctBlock.dctCoeffs);
            
            QuantizedBlock finalBlock(quantized, dctBlock.x, dctBlock.y, dctBlock.component);
            
            unique_lock<mutex> finalLock(finalMutex);
            finalBlocks.push_back(move(finalBlock));
        }
    }
}

// ========== PipelineJpegEncoder ==========

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
