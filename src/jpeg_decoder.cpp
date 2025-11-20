#include "jpeg_decoder.h"
#include "sequential_processors.h"
#include "bit_reader.h"
#include "dct_math.h"
#include "color_math.h"
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace std;

// Zigzag индексы для обратного преобразования
static const int zigzagOrder[64] = {
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

// ========== SequentialDctInverseTransform ==========

vector<vector<double>> SequentialDctInverseTransform::inverseDct(
    const vector<vector<int>>& quantizedBlock) {
    
    vector<vector<double>> result(8, vector<double>(8));
    
    // Кеш косинусов
    static vector<vector<double>> cosineCache;
    static bool cacheInitialized = false;
    
    if (!cacheInitialized) {
        cosineCache.resize(8, vector<double>(8));
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                cosineCache[i][j] = cos((2 * i + 1) * j * M_PI / 16.0);
            }
        }
        cacheInitialized = true;
    }
    
    // Формула обратного DCT
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double sum = 0.0;
            
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    double alpha_u = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    double alpha_v = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
                    
                    sum += alpha_u * alpha_v * quantizedBlock[u][v] * 
                           cosineCache[x][u] * cosineCache[y][v];
                }
            }
            
            result[x][y] = sum / 4.0;
        }
    }
    
    return result;
}

// ========== JpegDecoder ==========

JpegDecoder::JpegDecoder(vector<vector<int>> quantTable)
    : idct(make_unique<SequentialDctInverseTransform>()),
      quantizationTable(move(quantTable)) {}

vector<vector<int>> JpegDecoder::dequantize(const vector<vector<int>>& quantized) {
    vector<vector<int>> result(8, vector<int>(8));
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            result[i][j] = quantized[i][j] * quantizationTable[i][j];
        }
    }
    
    return result;
}

vector<vector<int>> JpegDecoder::inverseZigzag(const vector<int>& zigzagData) {
    vector<vector<int>> block(8, vector<int>(8, 0));
    
    for (int i = 0; i < 64 && i < static_cast<int>(zigzagData.size()); i++) {
        int index = zigzagOrder[i];
        int row = index / 8;
        int col = index % 8;
        block[row][col] = zigzagData[i];
    }
    
    return block;
}

RgbImage JpegDecoder::ycbcrToRgb(const YCbCrImage& ycbcr) {
    int width = ycbcr.getWidth();
    int height = ycbcr.getHeight();
    RgbImage rgb(width, height);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto [yVal, cbVal, crVal] = ycbcr.getPixel(x, y);
            
            // YCbCr to RGB conversion (ITU-R BT.601)
            double yD = yVal;
            double cbD = cbVal - 128.0;
            double crD = crVal - 128.0;
            
            double r = yD + 1.402 * crD;
            double g = yD - 0.344136 * cbD - 0.714136 * crD;
            double b = yD + 1.772 * cbD;
            
            // Clamp to 0-255
            r = max(0.0, min(255.0, r));
            g = max(0.0, min(255.0, g));
            b = max(0.0, min(255.0, b));
            
            rgb.setPixel(x, y, 
                        static_cast<unsigned char>(round(r)),
                        static_cast<unsigned char>(round(g)),
                        static_cast<unsigned char>(round(b)));
        }
    }
    
    return rgb;
}

void JpegDecoder::placeBlock(YCbCrImage& image, const vector<vector<double>>& block,
                            int blockX, int blockY, int component) {
    int width = image.getWidth();
    int height = image.getHeight();
    
    // Определяем шаг в зависимости от компонента (Y = 8, Cb/Cr = 16 из-за субсэмплинга)
    int step = (component == 0) ? 8 : 16;
    int pixelX = blockX * step;
    int pixelY = blockY * step;
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            // Восстанавливаем значение (добавляем 128)
            double val = block[i][j] + 128.0;
            val = max(0.0, min(255.0, val));
            unsigned char byteVal = static_cast<unsigned char>(round(val));
            
            if (component == 0) {
                // Y компонент - полное разрешение
                int px = pixelX + j;
                int py = pixelY + i;
                if (px < width && py < height) {
                    auto [oldY, oldCb, oldCr] = image.getPixel(px, py);
                    image.setPixel(px, py, byteVal, oldCb, oldCr);
                }
            } else {
                // Cb/Cr компоненты - субсэмплинг 2x2
                // Каждый пиксель блока соответствует 2x2 пикселям изображения
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int px = pixelX + j * 2 + dx;
                        int py = pixelY + i * 2 + dy;
                        if (px < width && py < height) {
                            auto [oldY, oldCb, oldCr] = image.getPixel(px, py);
                            if (component == 1) {
                                image.setPixel(px, py, oldY, byteVal, oldCr);
                            } else {
                                image.setPixel(px, py, oldY, oldCb, byteVal);
                            }
                        }
                    }
                }
            }
        }
    }
}

RgbImage JpegDecoder::decode(const JpegEncodedData& encodedData) {
    // Полное декодирование из Huffman потока - сложная задача
    // Для тестирования используем decodeFromBlocks
    // Эта функция оставлена как placeholder для полной реализации
    
    cerr << "Warning: Full Huffman decoding not implemented. "
         << "Use decodeFromBlocks() for testing." << endl;
    
    // Возвращаем пустое изображение
    return RgbImage(encodedData.width, encodedData.height);
}

RgbImage JpegDecoder::decodeFromBlocks(const vector<QuantizedBlock>& blocks, 
                                       int width, int height) {
    // Создаем YCbCr изображение
    YCbCrImage ycbcr(width, height);
    
    // Инициализируем значениями по умолчанию (серый)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            ycbcr.setPixel(x, y, 128, 128, 128);
        }
    }
    
    // Обрабатываем каждый блок
    for (const auto& block : blocks) {
        // Получаем коэффициенты в формате 8x8
        auto quantized = block.toArray();
        
        // Деквантизация
        auto dequantized = dequantize(quantized);
        
        // Обратное DCT
        auto spatial = idct->inverseDct(dequantized);
        
        // Помещаем блок в изображение
        placeBlock(ycbcr, spatial, block.getBlockX(), block.getBlockY(), block.getComponent());
    }
    
    // Конвертируем YCbCr в RGB
    return ycbcrToRgb(ycbcr);
}

// ========== Фабричные функции ==========

unique_ptr<JpegEncoder> createJpegEncoder(int quality) {
    auto colorConverter = make_unique<SequentialColorConverter>();
    auto dctTransform = make_unique<SequentialDctTransform>();
    auto quantizer = make_unique<SequentialQuantizer>(quality);
    auto blockProcessor = make_unique<SequentialBlockProcessor>(
        move(dctTransform), move(quantizer));
    auto huffmanEncoder = make_unique<SequentialHuffmanEncoder>();
    
    return make_unique<JpegEncoder>(
        move(colorConverter), move(blockProcessor), move(huffmanEncoder));
}

unique_ptr<JpegDecoder> createJpegDecoder(const vector<vector<int>>& quantTable) {
    return make_unique<JpegDecoder>(quantTable);
}
