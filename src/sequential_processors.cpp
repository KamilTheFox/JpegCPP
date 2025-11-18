#include "sequential_processors.h"
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace std;

// SequentialBlockProcessor конструктор
SequentialBlockProcessor::SequentialBlockProcessor(unique_ptr<IDctTransform> dctTransform, 
                                                 unique_ptr<IQuantizer> quantizer)
    : dct(move(dctTransform)), quantizer(move(quantizer)) {}

vector<QuantizedBlock> SequentialBlockProcessor::processBlocks(const YCbCrImage& image) {
    vector<QuantizedBlock> blocks;
    
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Обрабатываем Y (luminance) компоненты - полное разрешение
    for (int by = 0; by < height; by += 8) {
        for (int bx = 0; bx < width; bx += 8) {
            // Y block (luminance)
            auto yBlock = extractBlock(image, bx, by, 0); // component 0 = Y
            auto yDctBlock = dct->forwardDct(yBlock);
            auto yQuantized = quantizer->quantize(yDctBlock);
            blocks.emplace_back(yQuantized, bx / 8, by / 8, 0);
        }
    }
    
    // Обрабатываем Cb компоненты - субсэмплинг 2x2
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            // Cb block (chroma blue)
            auto cbBlock = extractBlock(image, bx, by, 1); // component 1 = Cb
            auto cbDctBlock = dct->forwardDct(cbBlock);
            auto cbQuantized = quantizer->quantize(cbDctBlock);
            blocks.emplace_back(cbQuantized, bx / 16, by / 16, 1);
        }
    }
    
    // Обрабатываем Cr компоненты - субсэмплинг 2x2  
    for (int by = 0; by < height; by += 16) {
        for (int bx = 0; bx < width; bx += 16) {
            // Cr block (chroma red)
            auto crBlock = extractBlock(image, bx, by, 2); // component 2 = Cr
            auto crDctBlock = dct->forwardDct(crBlock);
            auto crQuantized = quantizer->quantize(crDctBlock);
            blocks.emplace_back(crQuantized, bx / 16, by / 16, 2);
        }
    }
    
    return blocks;
}

vector<vector<double>> SequentialBlockProcessor::extractBlock(const YCbCrImage& image, 
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

// SequentialColorConverter
YCbCrImage SequentialColorConverter::convert(const RgbImage& image) {
    YCbCrImage result(image.getWidth(), image.getHeight());
    
    for (int y = 0; y < image.getHeight(); y++) {
        for (int x = 0; x < image.getWidth(); x++) {
            auto rgb = image.getPixel(x, y);
            auto ycbcr = ColorMath::rgbToYCbCr(get<0>(rgb), get<1>(rgb), get<2>(rgb));
            result.setPixel(x, y, get<0>(ycbcr), get<1>(ycbcr), get<2>(ycbcr));
        }
    }
    
    return result;
}

// SequentialDctTransform
vector<vector<double>> SequentialDctTransform::forwardDct(const vector<vector<double>>& block) {
    vector<vector<double>> result(8, vector<double>(8));
    
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            result[u][v] = DctMath::computeDctCoefficient(block, u, v);
        }
    }
    
    return result;
}

// SequentialHuffmanEncoder
JpegEncodedData SequentialHuffmanEncoder::encode(const vector<QuantizedBlock>& blocks, 
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
    
    // Разделяем блоки по компонентам
    vector<QuantizedBlock> yBlocks, cbBlocks, crBlocks;
    for (const auto& block : blocks) {
        switch (block.getComponent()) {
            case 0: yBlocks.push_back(block); break;
            case 1: cbBlocks.push_back(block); break;
            case 2: crBlocks.push_back(block); break;
        }
    }
    
    // Создаем отдельные таблицы Хаффмана для каждого компонента
    auto yTable = buildHuffmanTable(yBlocks);
    auto cbTable = buildHuffmanTable(cbBlocks);
    auto crTable = buildHuffmanTable(crBlocks);
    
    // Кодируем все блоки
    BitWriter writer;
    
    // Кодируем Y компоненты
    for (const auto& block : yBlocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = yTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    // Кодируем Cb компоненты
    for (const auto& block : cbBlocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = cbTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    // Кодируем Cr компоненты
    for (const auto& block : crBlocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            auto codeInfo = crTable[coef];
            writer.writeBits(codeInfo.first, codeInfo.second);
        }
    }
    
    return JpegEncodedData{
        writer.toArray(),
        yTable, // DC table для Y
        yTable, // AC table для Y (упрощение)
        quantTable,
        width,
        height
    };
}

unordered_map<int, pair<int, int>> SequentialHuffmanEncoder::buildHuffmanTable(
    const vector<QuantizedBlock>& blocks) {
    
    unordered_map<int, int> frequencies;
    
    for (const auto& block : blocks) {
        auto zigzag = block.getZigzagOrder();
        for (auto coef : zigzag) {
            frequencies[coef]++;
        }
    }
    
    auto tree = HuffmanMath::buildTree(frequencies);
    auto codeTable = HuffmanMath::buildCodeTable(tree);
    delete tree;
    
    return codeTable;
}

void SequentialHuffmanEncoder::encodeBlock(BitWriter& writer, const vector<int>& zigzag,
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

int SequentialHuffmanEncoder::getCategory(int value) {
    int absValue = abs(value);
    if (absValue == 0) return 0;
    return static_cast<int>(floor(log2(absValue))) + 1;
}

int SequentialHuffmanEncoder::getMagnitude(int value, int category) {
    if (value >= 0)
        return value;
    return value + (1 << category) - 1;
}

// SequentialQuantizer
SequentialQuantizer::SequentialQuantizer(int quality) 
    : quantizationTable(generateQuantizationTable(quality)) {}

vector<vector<int>> SequentialQuantizer::quantize(const vector<vector<double>>& dctBlock) {
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

vector<vector<int>> SequentialQuantizer::defaultQuantizationTable() {
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

vector<vector<int>> SequentialQuantizer::generateQuantizationTable(int quality) {
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

// JpegEncoder
JpegEncoder::JpegEncoder(unique_ptr<IColorConverter> colorConv,
                        unique_ptr<IBlockProcessor> blockProc,
                        unique_ptr<IHuffmanEncoder> huffmanEnc)
    : colorConverter(move(colorConv)),
      blockProcessor(move(blockProc)),
      encoder(move(huffmanEnc)) {}

JpegEncodedData JpegEncoder::encode(const RgbImage& image) {
    auto ycbcr = colorConverter->convert(image);
    auto blocks = blockProcessor->processBlocks(ycbcr);
    auto quantTable = SequentialQuantizer::defaultQuantizationTable();
    
    return encoder->encode(blocks, image.getWidth(), image.getHeight(), quantTable);
}